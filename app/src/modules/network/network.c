/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#include <zephyr/net/conn_mgr_monitor.h>
#include <zephyr/task_wdt/task_wdt.h>
#include <date_time.h>
#include <zephyr/smf.h>

#include "modem/lte_lc.h"
#include "modem/modem_info.h"
#include "modules_common.h"
#include "conn_info_object_encode.h"
#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(network, CONFIG_APP_NETWORK_LOG_LEVEL);

/* Register subscriber */
ZBUS_MSG_SUBSCRIBER_DEFINE(network);

/* Observe trigger channel */
ZBUS_CHAN_ADD_OBS(TRIGGER_CHAN, network, 0);
ZBUS_CHAN_ADD_OBS(TIME_CHAN, network, 0);
ZBUS_CHAN_ADD_OBS(NETWORK_CHAN, network, 0);

#define MAX_MSG_SIZE (MAX(MAX(sizeof(enum trigger_type), sizeof(enum time_status)), \
			      sizeof(enum network_status)))

/* Macros used to subscribe to specific Zephyr NET management events. */
#define L4_EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)
#define CONN_LAYER_EVENT_MASK (NET_EVENT_CONN_IF_FATAL_ERROR)

/* Zephyr NET management event callback structures. */
static struct net_mgmt_event_callback l4_cb;
static struct net_mgmt_event_callback conn_cb;


/* State machine */

/* Module states.
 *
 * STATE_INIT: The module is initializing and waiting for time to be available.
 * STATE_SAMPLING: The module is ready to sample upon receiving a trigger.
 * STATE_DISCONNECTED: The module is disconnected from the network, sampling is blocked.
 */
enum network_module_state {
	STATE_INIT,
	STATE_SAMPLING,
	STATE_WAIT_FOR_NETWORK_DISCONNECT,
	STATE_DISCONNECTED,
};

/* State object.
 * Used to transfer data between state changes.
 */
struct s_object {
	/* This must be first */
	struct smf_ctx ctx;

	/* Last channel type that a message was received on */
	const struct zbus_channel *chan;

	/* Buffer for last zbus message */
	uint8_t msg_buf[MAX_MSG_SIZE];
};
/* Forward declarations of state handlers */
static enum smf_state_result state_init_run(void *o);
static enum smf_state_result state_sampling_run(void *o);
static void state_disconnected_entry(void *o);
static enum smf_state_result state_wait_for_network_disconnect_run(void *o);

static struct s_object s_obj;
static const struct smf_state states[] = {
	[STATE_INIT] =
		SMF_CREATE_STATE(NULL, state_init_run, NULL,
				 NULL,	/* No parent state */
				 NULL), /* No initial transition */
	[STATE_SAMPLING] =
		SMF_CREATE_STATE(NULL, state_sampling_run, NULL,
				 NULL,
				 NULL),
	[STATE_WAIT_FOR_NETWORK_DISCONNECT] =
		SMF_CREATE_STATE(NULL, state_wait_for_network_disconnect_run, NULL,
				 NULL,
				 NULL),
	[STATE_DISCONNECTED] =
		SMF_CREATE_STATE(state_disconnected_entry, NULL, NULL,
				 NULL,
				 NULL),
};

static void network_status_notify(enum network_status status)
{
	int err;

	err = zbus_chan_pub(&NETWORK_CHAN, &status, K_SECONDS(1));
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}
}

static void l4_event_handler(struct net_mgmt_event_callback *cb,
			     uint64_t event,
			     struct net_if *iface)
{
	switch (event) {
	case NET_EVENT_L4_CONNECTED:
		LOG_INF("Network connectivity established");
		network_status_notify(NETWORK_CONNECTED);
		break;
	case NET_EVENT_L4_DISCONNECTED:
		LOG_INF("Network connectivity lost");
		network_status_notify(NETWORK_DISCONNECTED);
		break;
	default:
		/* Don't care */
		return;
	}
}

static void connectivity_event_handler(struct net_mgmt_event_callback *cb,
				       uint64_t event,
				       struct net_if *iface)
{
	if (event == NET_EVENT_CONN_IF_FATAL_ERROR) {
		LOG_ERR("NET_EVENT_CONN_IF_FATAL_ERROR");
		SEND_FATAL_ERROR();
		return;
	}
}

#if IS_ENABLED(CONFIG_LTE_LINK_CONTROL)
static void lte_lc_evt_handler(const struct lte_lc_evt *const evt)
{
	switch (evt->type) {
	case LTE_LC_EVT_NW_REG_STATUS:
		if (evt->nw_reg_status == LTE_LC_NW_REG_UICC_FAIL) {
			LOG_ERR("No SIM card detected!");
			SEND_IRRECOVERABLE_ERROR();
			break;
		}
		break;
	case LTE_LC_EVT_MODEM_EVENT:
		/* If a reset loop happens in the field, it should not be necessary
		 * to perform any action. The modem will try to re-attach to the LTE network after
		 * the 30-minute block.
		 */
		if (evt->modem_evt.type == LTE_LC_MODEM_EVT_RESET_LOOP) {
			LOG_ERR("The modem has detected a reset loop!");
			SEND_IRRECOVERABLE_ERROR();
		}
		break;
	default:
		break;
	}
}
#endif /* IS_ENABLED(CONFIG_LTE_LINK_CONTROL) */

static void sample_network_quality(void)
{
	int64_t system_time;
	struct payload payload = { 0 };
	struct conn_info_object conn_info_obj = { 0 };
	int ret;

	struct lte_lc_conn_eval_params conn_eval_params;

	ret = lte_lc_conn_eval_params_get(&conn_eval_params);
	if (ret == -EOPNOTSUPP) {
		LOG_WRN("Connection evaluation not supported in current functional mode");
		return;
	} else if (ret < 0) {
		LOG_ERR("lte_lc_conn_eval_params_get, error: %d", ret);
		SEND_FATAL_ERROR();
		return;
	} else if (ret > 0) {
		LOG_WRN("Connection evaluation failed due to a network related reason: %d", ret);
		return;
	}

	ret = date_time_now(&system_time);
	if (ret) {
		LOG_ERR("Failed to convert uptime to unix time, error: %d", ret);
		return;
	}

	conn_info_obj.base_attributes_m.bt = (int32_t)(system_time / 1000);
	conn_info_obj.energy_estimate_m.vi = conn_eval_params.energy_estimate;

	if (conn_eval_params.rsrp == LTE_LC_CELL_RSRP_INVALID) {
		LOG_WRN("RSRP value is invalid, ignoring");
	} else {
		conn_info_obj.rsrp_m.vi_present = true;
		conn_info_obj.rsrp_m.vi.vi = RSRP_IDX_TO_DBM(conn_eval_params.rsrp);

		LOG_DBG("RSRP: %d dBm", conn_info_obj.rsrp_m.vi.vi);
	}

	LOG_DBG("System Time: %d", conn_info_obj.base_attributes_m.bt);
	LOG_DBG("Energy Estimate: %d", conn_info_obj.energy_estimate_m.vi);

	ret = cbor_encode_conn_info_object(payload.buffer, sizeof(payload.buffer),
					   &conn_info_obj, &payload.buffer_len);
	if (ret) {
		LOG_ERR("Failed to encode conn info object, error: %d", ret);
		SEND_FATAL_ERROR();
		return;
	}

	LOG_DBG("Submitting payload");

	int err = zbus_chan_pub(&PAYLOAD_CHAN, &payload, K_SECONDS(1));
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}
}

/* State handlers */

static enum smf_state_result state_init_run(void *obj)
{
	struct s_object const *state_object = obj;

	if (&TIME_CHAN == state_object->chan) {
		enum time_status time_status = MSG_TO_TIME_STATUS(state_object->msg_buf);

		if (time_status == TIME_AVAILABLE) {
			LOG_DBG("Time available, sampling can start");

			STATE_SET(STATE_SAMPLING);
			return SMF_EVENT_HANDLED;
		}
	}

	return SMF_EVENT_PROPAGATE;
}


static enum smf_state_result state_sampling_run(void *obj)
{
	struct s_object const *state_object = obj;

	if (&TRIGGER_CHAN == state_object->chan) {
		enum trigger_type trigger_type = MSG_TO_TRIGGER_TYPE(state_object->msg_buf);

		if (trigger_type == TRIGGER_DATA_SAMPLE) {
			LOG_DBG("Data sample trigger received, getting network quality data");

			if (IS_ENABLED(CONFIG_APP_NETWORK_SAMPLE_NETWORK_QUALITY)) {
				sample_network_quality();
			}
		}
	}

	if (&NETWORK_CHAN == state_object->chan) {
		enum network_status status = MSG_TO_NETWORK_STATUS(state_object->msg_buf);

		if (status == NETWORK_DISCONNECT_REQUEST) {
			LOG_DBG("Network disconnect request request received");

			int err = conn_mgr_all_if_disconnect(true);

			if (err) {
				LOG_ERR("conn_mgr_all_if_disconnect, error: %d", err);
				SEND_FATAL_ERROR();
			}

			STATE_SET(STATE_WAIT_FOR_NETWORK_DISCONNECT);
			return SMF_EVENT_HANDLED;
		}
	}

	return SMF_EVENT_PROPAGATE;
}

static enum smf_state_result state_wait_for_network_disconnect_run(void *o)
{
	struct s_object *state_object = o;

	if (&NETWORK_CHAN == state_object->chan) {
		const enum network_status status = MSG_TO_NETWORK_STATUS(state_object->msg_buf);

		if (status == NETWORK_DISCONNECTED) {
			LOG_DBG("Network disconnected, sampling is blocked");

			STATE_SET(STATE_DISCONNECTED);
			return SMF_EVENT_HANDLED;
		}
	}

	return SMF_EVENT_PROPAGATE;
}

static void state_disconnected_entry(void *obj)
{
	ARG_UNUSED(obj);

	LOG_DBG("state_disconnected_entry");
}

static void task_wdt_callback(int channel_id, void *user_data)
{
	LOG_ERR("Watchdog expired, Channel: %d, Thread: %s",
		channel_id, k_thread_name_get((k_tid_t)user_data));

	SEND_FATAL_ERROR_WATCHDOG_TIMEOUT();
}


static void network_task(void)
{
	int err;

	STATE_SET_INITIAL(STATE_INIT);

#if IS_ENABLED(CONFIG_LTE_LINK_CONTROL)
	/* Setup Link Controller Event handler */
	lte_lc_register_handler(lte_lc_evt_handler);
#endif /* IS_ENABLED(CONFIG_LTE_LINK_CONTROL) */

	/* Setup handler for Zephyr NET Connection Manager events. */
	net_mgmt_init_event_callback(&l4_cb, l4_event_handler, L4_EVENT_MASK);
	net_mgmt_add_event_callback(&l4_cb);

	/* Setup handler for Zephyr NET Connection Manager Connectivity layer. */
	net_mgmt_init_event_callback(&conn_cb, connectivity_event_handler, CONN_LAYER_EVENT_MASK);
	net_mgmt_add_event_callback(&conn_cb);

	/* Connecting to the configured connectivity layer.
	 * Wi-Fi or LTE depending on the board that the sample was built for.
	 */
	LOG_INF("Bringing network interface up and connecting to the network");

	err = conn_mgr_all_if_up(true);
	if (err) {
		LOG_ERR("conn_mgr_all_if_up, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	err = conn_mgr_all_if_connect(true);
	if (err) {
		LOG_ERR("conn_mgr_all_if_connect, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	network_status_notify(NETWORK_DISCONNECTED);

	if (IS_ENABLED(CONFIG_LTE_LINK_CONTROL)) {
		/* Subscribe to modem events */
		err = lte_lc_modem_events_enable();
		if (err) {
			LOG_ERR("lte_lc_modem_events_enable, error: %d", err);
			SEND_FATAL_ERROR();
			return;
		}
	}

	/* Resend connection status if the sample is built for Native Posix.
	 * This is necessary because the network interface is automatically brought up
	 * at SYS_INIT() before main() is called.
	 * This means that NET_EVENT_L4_CONNECTED fires before the
	 * appropriate handler l4_event_handler() is registered.
	 */
	if (IS_ENABLED(CONFIG_BOARD_NATIVE_POSIX)) {
		conn_mgr_mon_resend_status();
	}

	const uint32_t wdt_timeout_ms = (CONFIG_APP_NETWORK_WATCHDOG_TIMEOUT_SECONDS * MSEC_PER_SEC);
	const uint32_t execution_time_ms = (CONFIG_APP_NETWORK_EXEC_TIME_SECONDS_MAX * MSEC_PER_SEC);
	const k_timeout_t zbus_wait_ms = K_MSEC(wdt_timeout_ms - execution_time_ms);
	int task_wdt_id;

	task_wdt_id = task_wdt_add(wdt_timeout_ms, task_wdt_callback, (void *)k_current_get());

	while (true) {
		err = task_wdt_feed(task_wdt_id);
		if (err) {
			LOG_ERR("task_wdt_feed, error: %d", err);
			SEND_FATAL_ERROR();
			return;
		}

		err = zbus_sub_wait_msg(&network, &s_obj.chan, s_obj.msg_buf, zbus_wait_ms);
		if (err == -ENOMSG) {
			continue;
		} else if (err) {
			LOG_ERR("zbus_sub_wait_msg, error: %d", err);
			SEND_FATAL_ERROR();
			return;
		}

		err = STATE_RUN();
		if (err) {
			LOG_ERR("handle_message, error: %d", err);
			SEND_FATAL_ERROR();
			return;
		}
	}
}

K_THREAD_DEFINE(network_task_id,
		CONFIG_APP_NETWORK_THREAD_STACK_SIZE,
		network_task, NULL, NULL, NULL, K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);
