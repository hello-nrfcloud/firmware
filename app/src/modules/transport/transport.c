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
#include <date_time.h>

#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(transport, CONFIG_APP_TRANSPORT_LOG_LEVEL);

/* Register subscriber */
ZBUS_SUBSCRIBER_DEFINE(transport, CONFIG_APP_TRANSPORT_MESSAGE_QUEUE_SIZE);

/* Forward declarations */
static const struct smf_state state[];
static void connect_work_fn(struct k_work *work);

/* Define connection work - Used to handle reconnection attempts to the MQTT broker */
static K_WORK_DELAYABLE_DEFINE(connect_work, connect_work_fn);

/* Define stack_area of application workqueue */
K_THREAD_STACK_DEFINE(stack_area, CONFIG_APP_TRANSPORT_WORKQUEUE_STACK_SIZE);

/* Declare application workqueue. This workqueue is used to call mqtt_helper_connect(), and
 * schedule reconnectionn attempts upon network loss or disconnection from MQTT.
 */
static struct k_work_q transport_queue;

/* Semaphore to mark when we have a valid time */
K_SEM_DEFINE(date_time_ready_sem, 0, 1);

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
	struct payload payload;
} s_obj;

/* Connect work - Used to establish a connection to the MQTT broker and schedule reconnection
 * attempts.
 */
static void connect_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);

	int err;
	char buf[NRF_CLOUD_CLIENT_ID_MAX_LEN];

	err = nrf_cloud_client_id_get(buf, sizeof(buf));
	if (!err) {
		LOG_INF("Connecting to nRF Cloud CoAP with client ID: %s", buf);
	} else {
		LOG_ERR("nrf_cloud_client_id_get, error: %d", err);
		SEND_FATAL_ERROR();
	}

	err = nrf_cloud_coap_connect(NULL);
	if (err)
	{
		LOG_ERR("nrf_cloud_coap_connect, error: %d", err);
		goto retry;
	} else {
		LOG_INF("Connected to Cloud");
	}

	struct nrf_cloud_svc_info_ui ui_info = {
	    .gnss = true,
	};
	struct nrf_cloud_svc_info service_info = {
	    .ui = &ui_info};
	struct nrf_cloud_modem_info modem_info = {
	    .device = NRF_CLOUD_INFO_SET,
	    .network = NRF_CLOUD_INFO_SET,
	};
	struct nrf_cloud_device_status device_status = {
	    .modem = &modem_info,
	    .svc = &service_info};

	/* sending device info to shadow */
	err = nrf_cloud_coap_shadow_device_status_update(&device_status);
	if (err)
	{
		LOG_ERR("nrf_cloud_coap_shadow_device_status_update, error: %d", err);
		SEND_FATAL_ERROR();
	} else {
		smf_set_state(SMF_CTX(&s_obj), &state[CLOUD_CONNECTED]);
		return;
	}

retry:
	k_work_reschedule_for_queue(&transport_queue, &connect_work,
			  K_SECONDS(CONFIG_APP_TRANSPORT_RECONNECTION_TIMEOUT_SECONDS));
}

/* Zephyr State Machine framework handlers */

/* Function executed when the module enters the disconnected state. */
static void disconnected_entry(void *o)
{
	struct s_object *user_object = o;
	int err;

	enum cloud_status status = CLOUD_DISCONNECTED;
	err = zbus_chan_pub(&CLOUD_CHAN, &status, K_NO_WAIT);

	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
	}

	/* Reschedule a connection attempt if we are connected to network and we enter the
	 * disconnected state.
	 */
	if (user_object->status == NETWORK_CONNECTED) {
		k_work_reschedule_for_queue(&transport_queue, &connect_work, K_NO_WAIT);
	} else {
		err = nrf_cloud_coap_disconnect();
		if (err && (err != -ENOTCONN)) {
			LOG_ERR("nrf_cloud_coap_disconnect, error: %d", err);
			SEND_FATAL_ERROR();
		}
	}
}

/* Function executed when the module is in the disconnected state. */
static void disconnected_run(void *o)
{
	struct s_object *user_object = o;

	if ((user_object->status == NETWORK_DISCONNECTED) && (user_object->chan == &NETWORK_CHAN)) {
		/* If NETWORK_DISCONNECTED is received after the MQTT connection is closed,
		 * we cancel the connect work if it is onging.
		 */
		k_work_cancel_delayable(&connect_work);
	}

	if ((user_object->status == NETWORK_CONNECTED) && (user_object->chan == &NETWORK_CHAN)) {

		/* Wait for 5 seconds to ensure that the network stack is ready before
		 * attempting to connect to MQTT. This delay is only needed when building for
		 * Wi-Fi.
		 */
		k_work_reschedule_for_queue(&transport_queue, &connect_work, K_SECONDS(5));
	}

	if (user_object->chan == &PAYLOAD_CHAN) {
		LOG_ERR("Discarding payload since we are not connected to cloud.");
	}
}

/* Function executed when the module enters the connected state. */
static void connected_entry(void *o)
{
	ARG_UNUSED(o);

	enum cloud_status status = CLOUD_CONNECTED;
	int err = zbus_chan_pub(&CLOUD_CHAN, &status, K_NO_WAIT);

	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
	}

	/* Cancel any ongoing connect work when we enter connected state */
	k_work_cancel_delayable(&connect_work);
}

/* Function executed when the module is in the connected state. */
static void connected_run(void *o)
{
	struct s_object *user_object = o;

	if ((user_object->status == NETWORK_DISCONNECTED) && (user_object->chan == &NETWORK_CHAN)) {
		smf_set_state(SMF_CTX(&s_obj), &state[CLOUD_DISCONNECTED]);
		return;
	}

	if (user_object->chan == &PAYLOAD_CHAN) {
		LOG_INF("Sending payload to cloud");
		nrf_cloud_coap_bytes_send(user_object->payload.string,
					  user_object->payload.string_len,
					  false);
	}
}

/* Function executed when the module exits the connected state. */
static void connected_exit(void *o)
{
	ARG_UNUSED(o);

	LOG_INF("Disconnected from Cloud");
}

/* Construct state table */
static const struct smf_state state[] = {
	[CLOUD_DISCONNECTED] = SMF_CREATE_STATE(disconnected_entry, disconnected_run, NULL),
	[CLOUD_CONNECTED] = SMF_CREATE_STATE(connected_entry, connected_run, connected_exit),
};

static void date_time_handler(const struct date_time_evt *evt) {
	if (evt->type != DATE_TIME_NOT_OBTAINED) {
		k_sem_give(&date_time_ready_sem);
	}
}

static void transport_task(void)
{
	int err;
	const struct zbus_channel *chan;
	enum network_status status;
	struct payload payload;

	/* Setup handler for date_time library */
	date_time_register_handler(date_time_handler);

	/* Initialize and start application workqueue.
	 * This workqueue can be used to offload tasks and/or as a timer when wanting to
	 * schedule functionality using the 'k_work' API.
	 */
	k_work_queue_init(&transport_queue);
	k_work_queue_start(&transport_queue, stack_area,
			   K_THREAD_STACK_SIZEOF(stack_area),
			   K_HIGHEST_APPLICATION_THREAD_PRIO,
			   NULL);

	/* Set initial state */
	smf_set_initial(SMF_CTX(&s_obj), &state[CLOUD_DISCONNECTED]);

	/* Wait for initial time */
	k_sem_take(&date_time_ready_sem, K_FOREVER);

	err = nrf_cloud_coap_init();
	if (err)
	{
		LOG_ERR("nrf_cloud_coap_init, error: %d", err);
		SEND_FATAL_ERROR();
	}

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

			/* connect/disconnect depending on network state */
			/* TODO: run provisioning service */
			err = smf_run_state(SMF_CTX(&s_obj));
			if (err) {
				LOG_ERR("smf_run_state, error: %d", err);
				SEND_FATAL_ERROR();
				return;
			}
		}

		if (&PAYLOAD_CHAN == chan) {

			err = zbus_chan_read(&PAYLOAD_CHAN, &payload, K_SECONDS(1));
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
		CONFIG_APP_TRANSPORT_THREAD_STACK_SIZE,
		transport_task, NULL, NULL, NULL, 3, 0, 0);
