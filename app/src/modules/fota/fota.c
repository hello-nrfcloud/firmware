/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/zbus/zbus.h>
#include <net/nrf_cloud_coap.h>
#include <net/nrf_cloud_fota_poll.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/task_wdt/task_wdt.h>
#include <zephyr/smf.h>

#include "message_channel.h"
#include "modules_common.h"

/* Register log module */
LOG_MODULE_REGISTER(fota, CONFIG_APP_FOTA_LOG_LEVEL);

void fota_callback(const struct zbus_channel *chan);

/* Register message subscriber - will be called everytime a channel that the module listens on
 * receives a new message.
 */
ZBUS_MSG_SUBSCRIBER_DEFINE(fota);

/* Observe channels */
ZBUS_CHAN_ADD_OBS(TRIGGER_CHAN, fota, 0);
ZBUS_CHAN_ADD_OBS(CLOUD_CHAN, fota, 0);

#define MAX_MSG_SIZE MAX(sizeof(enum trigger_type), sizeof(enum cloud_status))

/* FOTA support context */
static void fota_reboot(enum nrf_cloud_fota_reboot_status status);
static void fota_error(enum nrf_cloud_fota_status status, const char *const status_details);

/* State machine */

/* Defining modules states.
 * STATE_RUNNING: During the initialization, the module will check if there are any pending jobs
 *	       and process them. This may lead to the reboot function being called.
 *	STATE_WAIT_FOR_CLOUD: The FOTA module is initialized and waiting for cloud connection.
 *	STATE_WAIT_FOR_TRIGGER: The FOTA module is waiting for a trigger to poll for updates.
 *	STATE_POLL_AND_PROCESS: The FOTA module is polling for updates and processing them.
 *	STATE_REBOOT_PENDING: The nRF Cloud FOTA library has requested a reboot to complete an update.
 */
enum fota_module_state {
	STATE_RUNNING,
	STATE_WAIT_FOR_CLOUD,
	STATE_WAIT_FOR_TRIGGER,
	STATE_POLL_AND_PROCESS,
	STATE_REBOOT_PENDING,
};

/* Enumerator to be used in private FOTA channel */
enum priv_fota_evt {
	/* FOTA processing is completed, no reboot is needed */
	FOTA_PRIV_PROCESSING_DONE,
	/* FOTA processing is completed, reboot is needed */
	FOTA_PRIV_REBOOT_PENDING,
};
/* User defined state object.
 * Used to transfer data between state changes.
 */
struct s_object {
	/* This must be first */
	struct smf_ctx ctx;

	/* Last channel type that a message was received on */
	const struct zbus_channel *chan;

	/* Buffer for last zbus message */
	uint8_t msg_buf[MAX_MSG_SIZE];

	/* FOTA context */
	struct nrf_cloud_fota_poll_ctx fota_ctx;
};

/* Private channel used to signal when FOTA processing is completed */
ZBUS_CHAN_DECLARE(PRIV_FOTA_CHAN);
ZBUS_CHAN_DEFINE(PRIV_FOTA_CHAN,
		 enum priv_fota_evt,
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS(fota),
		 ZBUS_MSG_INIT(0)
);


/* Forward declarations */
static void state_running_entry(void *o);
static void state_wait_for_cloud_run(void *o);
static void state_wait_for_trigger_run(void *o);
static void state_poll_and_process_entry(void *o);
static void state_poll_and_process_run(void *o);
static void state_poll_and_process_exit(void *o);
static void state_reboot_pending_entry(void *o);

static struct s_object s_obj = {
	.fota_ctx.reboot_fn = fota_reboot,
	.fota_ctx.error_fn = fota_error,
};

static const struct smf_state states[] = {
	[STATE_RUNNING] =
		SMF_CREATE_STATE(state_running_entry, NULL, NULL,
				 NULL,	/* No parent state */
				 &states[STATE_WAIT_FOR_CLOUD]),
	[STATE_WAIT_FOR_CLOUD] =
		SMF_CREATE_STATE(NULL, state_wait_for_cloud_run, NULL,
				 &states[STATE_RUNNING],
				 NULL), /* No initial transition */
	[STATE_WAIT_FOR_TRIGGER] =
		SMF_CREATE_STATE(NULL, state_wait_for_trigger_run, NULL,
				 &states[STATE_RUNNING],
				 NULL),
	[STATE_POLL_AND_PROCESS] =
		SMF_CREATE_STATE(state_poll_and_process_entry, state_poll_and_process_run,
				 state_poll_and_process_exit,
				 &states[STATE_RUNNING],
				 NULL),
	[STATE_REBOOT_PENDING] =
		SMF_CREATE_STATE(state_reboot_pending_entry, NULL, NULL,
				 &states[STATE_RUNNING],
				 NULL),
};

/* State handlers */

static void state_running_entry(void *o)
{
	struct s_object *state_object = o;

	/* Initialize the FOTA context */
	int err = nrf_cloud_fota_poll_init(&state_object->fota_ctx);
	if (err) {
		LOG_ERR("nrf_cloud_fota_poll_init failed: %d", err);
		SEND_FATAL_ERROR();
	}

	/* Process pending FOTA job, the FOTA type is returned */
	err = nrf_cloud_fota_poll_process_pending(&state_object->fota_ctx);
	if (err < 0) {
		LOG_ERR("nrf_cloud_fota_poll_process_pending failed: %d", err);
	} else if (err != NRF_CLOUD_FOTA_TYPE__INVALID) {
		LOG_INF("Processed pending FOTA job type: %d", err);
	}
}

static void state_wait_for_cloud_run(void *o)
{
	struct s_object *state_object = o;

	if (&CLOUD_CHAN == state_object->chan) {
		const enum cloud_status status = MSG_TO_CLOUD_STATUS(state_object->msg_buf);

		if (status == CLOUD_CONNECTED_READY_TO_SEND) {
			STATE_SET(STATE_WAIT_FOR_TRIGGER);
		}
	}
}

static void state_wait_for_trigger_run(void *o)
{
	struct s_object *state_object = o;

	if (&TRIGGER_CHAN == state_object->chan) {
		const enum trigger_type trigger_type = MSG_TO_TRIGGER_TYPE(state_object->msg_buf);

		if (trigger_type == TRIGGER_FOTA_POLL) {
			STATE_SET(STATE_POLL_AND_PROCESS);
		}
	}

	if (&CLOUD_CHAN == state_object->chan) {
		const enum cloud_status status = MSG_TO_CLOUD_STATUS(state_object->msg_buf);

		if (status == CLOUD_CONNECTED_PAUSED) {
			STATE_SET(STATE_WAIT_FOR_CLOUD);
		}
	}
}

static void state_poll_and_process_entry(void *o)
{
	struct s_object *state_object = o;
	/* Notify the rest of the system that FOTA processing is starting */
	enum fota_status fota_status = FOTA_STATUS_PROCESSING_START;

	int err = zbus_chan_pub(&FOTA_STATUS_CHAN, &fota_status, K_NO_WAIT);
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
	}

	/* Start the FOTA processing */
	err = nrf_cloud_fota_poll_process(&state_object->fota_ctx);
	if (err) {
		enum priv_fota_evt evt = FOTA_PRIV_PROCESSING_DONE;

		if (err != -EAGAIN) {
			LOG_ERR("nrf_cloud_fota_poll_process failed: %d", err);
		}

		err = zbus_chan_pub(&PRIV_FOTA_CHAN, &evt, K_NO_WAIT);
		if (err) {
			LOG_ERR("zbus_chan_pub, error: %d", err);
			SEND_FATAL_ERROR();
		}
	}
}

static void state_poll_and_process_run(void *o)
{
	struct s_object *state_object = o;

	if (&PRIV_FOTA_CHAN == state_object->chan) {
		const enum priv_fota_evt evt = *(const enum priv_fota_evt *)state_object->msg_buf;

		switch (evt) {
		case FOTA_PRIV_PROCESSING_DONE: {
			STATE_SET(STATE_WAIT_FOR_TRIGGER);
			break;
		}
		case FOTA_PRIV_REBOOT_PENDING:
			/* The FOTA module has requested a reboot to complete an update */
			STATE_SET(STATE_REBOOT_PENDING);
			break;
		default:
			LOG_ERR("Unknown event: %d", evt);
			break;
		}
	}
}

static void state_poll_and_process_exit(void *o)
{
	enum fota_status fota_status = FOTA_STATUS_PROCESSING_DONE;

	ARG_UNUSED(o);

	int err = zbus_chan_pub(&FOTA_STATUS_CHAN, &fota_status, K_NO_WAIT);
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
	}
}

static void state_reboot_pending_entry(void *o)
{
	ARG_UNUSED(o);

	int err;
	enum fota_status fota_status = FOTA_STATUS_REBOOT_PENDING;

	/* Notify the rest of the system that a reboot is pending */
	err = zbus_chan_pub(&FOTA_STATUS_CHAN, &fota_status, K_NO_WAIT);
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
	}

	/* Reboot the device */
	LOG_INF("Rebooting in %d seconds to complete FOTA process",
		CONFIG_APP_FOTA_REBOOT_DELAY_SECONDS);
	k_sleep(K_SECONDS(CONFIG_APP_FOTA_REBOOT_DELAY_SECONDS));
	sys_reboot(SYS_REBOOT_COLD);
}

/* End of state handlers */

static void task_wdt_callback(int channel_id, void *user_data)
{
	LOG_ERR("Watchdog expired, Channel: %d, Thread: %s",
		channel_id, k_thread_name_get((k_tid_t)user_data));

	SEND_FATAL_ERROR_WATCHDOG_TIMEOUT();
}

static void fota_reboot(enum nrf_cloud_fota_reboot_status status)
{
	int err;
	enum priv_fota_evt evt = FOTA_PRIV_REBOOT_PENDING;

	LOG_INF("Reboot requested with FOTA status %d", status);

	err = zbus_chan_pub(&PRIV_FOTA_CHAN, &evt, K_NO_WAIT);
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
	}

	/* We intentially don't reboot here as that will happen in the STATE_REBOOT_PENDING.
	 * Instead, we notify the rest of the system about the pending reboot first.
	 * That is done in the state's entry function.
	 */
}

static void fota_error(enum nrf_cloud_fota_status status, const char *const status_details)
{
	int err;
	enum priv_fota_evt evt = FOTA_PRIV_PROCESSING_DONE;

	LOG_ERR("FOTA error: %d, details: %s", status, status_details);

	err = zbus_chan_pub(&PRIV_FOTA_CHAN, &evt, K_NO_WAIT);
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
	}
}

static void fota_task(void)
{
	int err;
	int task_wdt_id;
	const uint32_t wdt_timeout_ms = (CONFIG_APP_FOTA_WATCHDOG_TIMEOUT_SECONDS * MSEC_PER_SEC);
	const uint32_t execution_time_ms = (CONFIG_APP_FOTA_EXEC_TIME_SECONDS_MAX * MSEC_PER_SEC);
	const k_timeout_t zbus_wait_ms = K_MSEC(wdt_timeout_ms - execution_time_ms);

	LOG_DBG("FOTA module task started");

	task_wdt_id = task_wdt_add(wdt_timeout_ms, task_wdt_callback, (void *)k_current_get());

	STATE_SET_INITIAL(STATE_RUNNING);

	while (true) {
		err = task_wdt_feed(task_wdt_id);
		if (err) {
			LOG_ERR("task_wdt_feed, error: %d", err);
			SEND_FATAL_ERROR();
			return;
		} else {
			LOG_DBG("Task WDT fed");
		}

		err = zbus_sub_wait_msg(&fota, &s_obj.chan, s_obj.msg_buf, zbus_wait_ms);
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

K_THREAD_DEFINE(fota_task_id,
		CONFIG_APP_FOTA_THREAD_STACK_SIZE,
		fota_task, NULL, NULL, NULL, 3, 0, 0);
