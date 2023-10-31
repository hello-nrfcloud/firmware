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
#include "deviceToCloud_encode.h"
#include "hex_coding/hex_coding.h"

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
enum module_state
{
	STATE_DISCONNECTED,
	STATE_CONNECTING,
	STATE_CONNECTED
};

#define RECONNECT_TIMEOUT_SECONDS 60

/* User defined state object.
 * Used to transfer data between state changes.
 */
static struct s_object
{
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
	LOG_INF("publish!");
	uint8_t buf[512];
	uint8_t buf2[1025] = {0};
	size_t out_len = 0;
	int ret = cbor_encode_deviceToCloud_message(buf, ARRAY_SIZE(buf), payload, &out_len);
	if (ret) {
		LOG_ERR("error encoding d2c message: %d", ret);
		return;
	}
	hex_encode(buf, buf2, out_len);

	ret = nrf_cloud_coap_message_send("42", buf2, false, NRF_CLOUD_NO_TIMESTAMP);
	if (ret) {
		LOG_ERR("error sending message to nRF Cloud CoAP: %d", ret);
	}
}

bool wait_for_zbus(void *o)
{
	struct s_object *user_object = o;
	int err;
	if (zbus_sub_wait(&transport, &s_obj.chan, K_FOREVER))
	{
		return false;
	}

	if (&NETWORK_CHAN == s_obj.chan)
	{

		err = zbus_chan_read(&NETWORK_CHAN, &s_obj.status, K_SECONDS(1));
		if (err)
		{
			LOG_ERR("zbus_chan_read, error: %d", err);
			SEND_FATAL_ERROR();
			return false;
		}
		return true;
	}

	if (&MSG_OUT_CHAN == s_obj.chan)
	{

		err = zbus_chan_read(&MSG_OUT_CHAN, &s_obj.payload, K_SECONDS(1));
		if (err)
		{
			LOG_ERR("zbus_chan_read, error: %d", err);
			SEND_FATAL_ERROR();
			return false;
		}
		return true;
	}
	return false;
}

/* Zephyr State Machine framework handlers */

/* Function executed when the module is in the disconnected state. */
static void disconnected_run(void *o)
{
	LOG_DBG("Disconnected");
	if (!wait_for_zbus(o))
	{
		return;
	}

	struct s_object *user_object = o;

	if ((user_object->status == NETWORK_CONNECTED) && (user_object->chan == &NETWORK_CHAN))
	{
		smf_set_state(SMF_CTX(&s_obj), &state[STATE_CONNECTING]);
	}
}

static void connecting_run(void *o)
{
	LOG_DBG("Connecting");
	int err = 0;
	struct s_object *user_object = o;
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

	if ((user_object->status == NETWORK_DISCONNECTED) && (user_object->chan == &NETWORK_CHAN))
	{
		smf_set_state(SMF_CTX(&s_obj), &state[STATE_DISCONNECTED]);

		int err = nrf_cloud_coap_disconnect();

		if (err)
		{
			LOG_ERR("nrf_cloud_coap_disconnect, error: %d", err);
			return;
		}
	}
	err = nrf_cloud_coap_connect();
	if (err)
	{
		LOG_ERR("nrf_cloud_coap_connect, error: %d", err);
		goto error;
	}

	err = nrf_cloud_coap_shadow_device_status_update(&device_status);
	if (err)
	{
		LOG_ERR("nrf_cloud_coap_shadow_device_status_update, error: %d", err);
		goto error;
	}
	smf_set_state(SMF_CTX(&s_obj), &state[STATE_CONNECTED]);
	return;
error:
	nrf_cloud_coap_disconnect();
	LOG_WRN("Retrying cloud connection in %d seconds.", RECONNECT_TIMEOUT_SECONDS);
	k_sleep(K_SECONDS(RECONNECT_TIMEOUT_SECONDS));
}

/* Function executed when the module is in the connected state. */
static void connected_run(void *o)
{
	LOG_DBG("Connected");
	struct s_object *user_object = o;

	if (!wait_for_zbus(o))
	{
		return;
	}

	if ((user_object->status == NETWORK_DISCONNECTED) && (user_object->chan == &NETWORK_CHAN))
	{
		smf_set_state(SMF_CTX(&s_obj), &state[STATE_DISCONNECTED]);

		int err = nrf_cloud_coap_disconnect();

		if (err)
		{
			LOG_ERR("nrf_cloud_coap_disconnect, error: %d", err);
			return;
		}
	}

	if (user_object->chan == &MSG_OUT_CHAN)
	{
		publish(&user_object->payload);
		return;
	}
}

/* Construct state table */
static const struct smf_state state[] = {
    [STATE_DISCONNECTED] = SMF_CREATE_STATE(NULL, disconnected_run, NULL),
    [STATE_CONNECTING] = SMF_CREATE_STATE(NULL, connecting_run, NULL),
    [STATE_CONNECTED] = SMF_CREATE_STATE(NULL, connected_run, NULL),
};

static void transport_task(void)
{
	int err;
	const struct zbus_channel *chan;
	enum network_status status;

	/* Initialize and start application workqueue.
	 * This workqueue can be used to offload tasks and/or as a timer when wanting to
	 * schedule functionality using the 'k_work' API.
	 */

	err = nrf_cloud_coap_init();
	if (err)
	{
		LOG_ERR("nrf_cloud_coap_init, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	/* Set initial state */
	smf_set_initial(SMF_CTX(&s_obj), &state[STATE_DISCONNECTED]);

	while (true)
	{
		err = smf_run_state(SMF_CTX(&s_obj));
		if (err)
		{
			LOG_ERR("smf_run_state, error: %d", err);
			SEND_FATAL_ERROR();
			return;
		}
	}
}

K_THREAD_DEFINE(transport_task_id,
		CONFIG_MQTT_SAMPLE_TRANSPORT_THREAD_STACK_SIZE,
		transport_task, NULL, NULL, NULL, 3, 0, 0);
