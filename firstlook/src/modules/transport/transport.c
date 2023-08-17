/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/smf.h>
#include <net/nrf_cloud.h>
#include <net/nrf_cloud_coap.h>
#include <modem/modem_info.h>

#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(transport, CONFIG_MQTT_SAMPLE_TRANSPORT_LOG_LEVEL);

/* Register subscriber */
ZBUS_SUBSCRIBER_DEFINE(transport, CONFIG_MQTT_SAMPLE_TRANSPORT_MESSAGE_QUEUE_SIZE);

/* Forward declarations */
static const struct smf_state state[];

/* Define stack_area of application workqueue */
K_THREAD_STACK_DEFINE(stack_area, CONFIG_MQTT_SAMPLE_TRANSPORT_WORKQUEUE_STACK_SIZE);

/* Declare application workqueue */
static struct k_work_q transport_queue;

/* Internal states */
enum module_state { STATE_NETWORK_CONNECTED, STATE_NETWORK_DISCONNECTED };

/* User defined state object.
 * Used to transfer data between state changes.
 */
static struct s_object {
	/* This must be first */
	struct smf_ctx ctx;

	/* Last channel type that a message was received on */
	const struct zbus_channel *chan;

	/* Network status */
	enum network_status status;

	/* Payload */
	struct deviceToCloud_message payload;
} s_obj;

/* Local convenience functions */

static void publish(struct deviceToCloud_message *payload)
{
	int err;

	LOG_INF("publish!");

	/* Publish location here. */
}

/* Zephyr State Machine framework handlers */

/* Function executed when the module is in the disconnected state. */
static void disconnected_run(void *o)
{
	int err = 0;
	struct s_object *user_object = o;
	struct nrf_cloud_svc_info_ui ui_info = {
		.gnss = true,
	};
	struct nrf_cloud_svc_info service_info = {
		.ui = &ui_info
	};
	struct nrf_cloud_modem_info modem_info = {
		.device = NRF_CLOUD_INFO_SET,
		.network = NRF_CLOUD_INFO_SET,
	};
	struct nrf_cloud_device_status device_status = {
		.modem = &modem_info,
		.svc = &service_info
	};

	if ((user_object->status == NETWORK_CONNECTED) && (user_object->chan == &NETWORK_CHAN)) {
		smf_set_state(SMF_CTX(&s_obj), &state[STATE_NETWORK_CONNECTED]);

		err = nrf_cloud_coap_connect();
		if (err) {
			LOG_ERR("nrf_cloud_coap_connect, error: %d", err);
			return;
		}

		err = nrf_cloud_coap_shadow_device_status_update(&device_status);
		if (err) {
			LOG_ERR("nrf_cloud_coap_shadow_device_status_update, error: %d", err);
			return;
		}
	}
}

/* Function executed when the module is in the connected state. */
static void connected_run(void *o)
{
	struct s_object *user_object = o;

	if ((user_object->status == NETWORK_DISCONNECTED) && (user_object->chan == &NETWORK_CHAN)) {
		smf_set_state(SMF_CTX(&s_obj), &state[STATE_NETWORK_DISCONNECTED]);

		int err = nrf_cloud_coap_disconnect();

		if (err) {
			LOG_ERR("nrf_cloud_coap_disconnect, error: %d", err);
			return;
		}
	}

	if (user_object->chan == &MSG_OUT_CHAN) {
		publish(&user_object->payload);
		return;
	}
}

/* Construct state table */
static const struct smf_state state[] = {
	[STATE_NETWORK_DISCONNECTED] = SMF_CREATE_STATE(NULL, disconnected_run, NULL),
	[STATE_NETWORK_CONNECTED] = SMF_CREATE_STATE(NULL, connected_run, NULL),
};

static void transport_task(void)
{
	int err;
	const struct zbus_channel *chan;
	enum network_status status;
	struct deviceToCloud_message payload;

	/* Initialize and start application workqueue.
	 * This workqueue can be used to offload tasks and/or as a timer when wanting to
	 * schedule functionality using the 'k_work' API.
	 */

	err = nrf_cloud_coap_init();
	if (err) {
		LOG_ERR("nrf_cloud_coap_init, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	/* Set initial state */
	smf_set_initial(SMF_CTX(&s_obj), &state[STATE_NETWORK_DISCONNECTED]);

	while (!zbus_sub_wait(&transport, &chan, K_FOREVER)) {

		s_obj.chan = chan;

		if (&NETWORK_CHAN == chan) {

			err = zbus_chan_read(&NETWORK_CHAN, &status, K_SECONDS(1));
			if (err) {
				LOG_ERR("zbus_chan_read, error: %d", err);
				SEND_FATAL_ERROR();
				return;
			}

			s_obj.status = status;

			err = smf_run_state(SMF_CTX(&s_obj));
			if (err) {
				LOG_ERR("smf_run_state, error: %d", err);
				SEND_FATAL_ERROR();
				return;
			}
		}

		if (&MSG_OUT_CHAN == chan) {

			err = zbus_chan_read(&MSG_OUT_CHAN, &payload, K_SECONDS(1));
			if (err) {
				LOG_ERR("zbus_chan_read, error: %d", err);
				SEND_FATAL_ERROR();
				return;
			}

			s_obj.payload = payload;

			err = smf_run_state(SMF_CTX(&s_obj));
			if (err) {
				LOG_ERR("smf_run_state, error: %d", err);
				SEND_FATAL_ERROR();
				return;
			}
		}
	}
}

K_THREAD_DEFINE(transport_task_id,
		CONFIG_MQTT_SAMPLE_TRANSPORT_THREAD_STACK_SIZE,
		transport_task, NULL, NULL, NULL, 3, 0, 0);
