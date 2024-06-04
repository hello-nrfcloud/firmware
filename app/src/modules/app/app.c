/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/task_wdt/task_wdt.h>
#include <net/nrf_cloud_coap.h>
#include <nrf_cloud_coap_transport.h>

#include "message_channel.h"
#include "app_object_decode.h" /* change this to config object and add led object here */

/* Register log module */
LOG_MODULE_REGISTER(app, CONFIG_APP_LOG_LEVEL);

/* Register subscriber */
ZBUS_SUBSCRIBER_DEFINE(app, CONFIG_APP_MODULE_MESSAGE_QUEUE_SIZE);

BUILD_ASSERT(CONFIG_APP_MODULE_WATCHDOG_TIMEOUT_SECONDS > CONFIG_APP_MODULE_EXEC_TIME_SECONDS_MAX,
	     "Watchdog timeout must be greater than maximum execution time");

static void shadow_get(bool get_desired)
{
	int err;
	struct app_object app_object = { 0 };
	uint8_t buf_cbor[512] = { 0 };
	uint8_t buf_json[512] = { 0 };
	size_t buf_cbor_len = sizeof(buf_cbor);
	size_t buf_json_len = sizeof(buf_json);
	size_t not_used;

	/* Request shadow delta, request only changes by setting delta parameter to true. */
	err = nrf_cloud_coap_shadow_get(buf_json, &buf_json_len, !get_desired,
					COAP_CONTENT_FORMAT_APP_JSON);
	if (err == -EACCES) {
		LOG_WRN("Not connected");
		return;
	} else if (err) {
		LOG_ERR("Failed to request shadow delta: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	if (buf_json_len == 0 || strlen(buf_json) == 0) {
		LOG_WRN("No shadow delta changes available");
		return;
	}

	/* Shadow available, fetch CBOR encoded desired section. */
	err = nrf_cloud_coap_shadow_get(buf_cbor, &buf_cbor_len, false,
					COAP_CONTENT_FORMAT_APP_CBOR);
	if (err == -EACCES) {
		LOG_WRN("Not connected");
		return;
	} else if (err) {
		LOG_ERR("Failed to request shadow delta: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	err = cbor_decode_app_object(buf_cbor, buf_cbor_len, &app_object, &not_used);
	if (err) {
		LOG_ERR("Ignoring incoming configuration change due to decoding error: %d", err);
		return;
	}

	struct configuration configuration = { 0 };

	if (app_object.lwm2m._1424010_present) {
		configuration.led_present = true;
		configuration.led_red = app_object.lwm2m._1424010._1424010._0._0;
		configuration.led_green = app_object.lwm2m._1424010._1424010._0._1;
		configuration.led_blue = app_object.lwm2m._1424010._1424010._0._2;

		LOG_DBG("LED object (1424010) values received from cloud:");
		LOG_DBG("R: %d", app_object.lwm2m._1424010._1424010._0._0);
		LOG_DBG("G: %d", app_object.lwm2m._1424010._1424010._0._1);
		LOG_DBG("B: %d", app_object.lwm2m._1424010._1424010._0._2);
		LOG_DBG("Timestamp: %lld", app_object.lwm2m._1424010._1424010._0._99);
	}

	if (app_object.lwm2m._1430110_present) {
		configuration.config_present = true;
		configuration.update_interval = app_object.lwm2m._1430110._1430110._0._0;
		configuration.gnss = app_object.lwm2m._1430110._1430110._0._1;

		LOG_DBG("Application configuration object (1430110) values received from cloud:");
		LOG_DBG("Update interval: %lld", app_object.lwm2m._1430110._1430110._0._0);
		LOG_DBG("GNSS: %d", app_object.lwm2m._1430110._1430110._0._1);
		LOG_DBG("Timestamp: %lld", app_object.lwm2m._1430110._1430110._0._99);
	}

	/* Distribute configuration */
	err = zbus_chan_pub(&CONFIG_CHAN, &configuration, K_SECONDS(1));
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	/* Send the received configuration back to the reported shadow section. */
	err = nrf_cloud_coap_shadow_state_update(buf_json);
	if (err < 0) {
		LOG_ERR("Failed to send PATCH request: %d", err);
	} else if (err > 0) {
		LOG_ERR("Error from server: %d", err);
	}
}

static void task_wdt_callback(int channel_id, void *user_data)
{
	LOG_ERR("Watchdog expired, Channel: %d, Thread: %s",
		channel_id, k_thread_name_get((k_tid_t)user_data));

	SEND_FATAL_ERROR();
}

static void app_task(void)
{
	int err;
	const struct zbus_channel *chan = NULL;
	int task_wdt_id;
	const uint32_t wdt_timeout_ms = (CONFIG_APP_MODULE_WATCHDOG_TIMEOUT_SECONDS * MSEC_PER_SEC);
	const uint32_t execution_time_ms = (CONFIG_APP_MODULE_EXEC_TIME_SECONDS_MAX * MSEC_PER_SEC);
	const k_timeout_t zbus_wait_ms = K_MSEC(wdt_timeout_ms - execution_time_ms);
	enum cloud_status cloud_status = 0;
	enum trigger_type trigger_type = 0;

	LOG_DBG("Application module task started");

	task_wdt_id = task_wdt_add(wdt_timeout_ms, task_wdt_callback, (void *)k_current_get());

	while (true) {
		err = task_wdt_feed(task_wdt_id);
		if (err) {
			LOG_ERR("task_wdt_feed, error: %d", err);
			SEND_FATAL_ERROR();
			return;
		}

		err = zbus_sub_wait(&app, &chan, zbus_wait_ms);
		if (err == -EAGAIN) {
			continue;
		} else if (err) {
			LOG_ERR("zbus_sub_wait, error: %d", err);
			SEND_FATAL_ERROR();
			return;
		}

		if (&CLOUD_CHAN == chan) {
			LOG_DBG("Cloud connection status received");

			err = zbus_chan_read(&CLOUD_CHAN, &cloud_status, K_FOREVER);
			if (err) {
				LOG_ERR("zbus_chan_read, error: %d", err);
				SEND_FATAL_ERROR();
				return;
			}

			if (cloud_status == CLOUD_CONNECTED_READY_TO_SEND) {
				LOG_DBG("Cloud connected");
				LOG_DBG("Getting latest device configuration from device shadow");

				shadow_get(true);
			}
		}

		if (&TRIGGER_CHAN == chan) {
			err = zbus_chan_read(&TRIGGER_CHAN, &trigger_type, K_FOREVER);
			if (err) {
				LOG_ERR("zbus_chan_read, error: %d", err);
				SEND_FATAL_ERROR();
				return;
			}

			if (trigger_type == TRIGGER_POLL) {
				LOG_DBG("Poll trigger received");
				LOG_DBG("Getting latest device configuration from device shadow");

				shadow_get(false);
			}
		}
	}
}

K_THREAD_DEFINE(app_task_id,
		CONFIG_APP_MODULE_THREAD_STACK_SIZE,
		app_task, NULL, NULL, NULL, 3, 0, 0);

static int watchdog_init(void)
{
	__ASSERT((task_wdt_init(NULL) == 0), "Task watchdog init failure");

	return 0;
}

SYS_INIT(watchdog_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
