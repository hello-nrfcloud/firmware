/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <simple_config/simple_config.h>
#include <zephyr/task_wdt/task_wdt.h>

#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(app, CONFIG_APP_LOG_LEVEL);

/* Register subscriber */
ZBUS_SUBSCRIBER_DEFINE(app, CONFIG_APP_MODULE_MESSAGE_QUEUE_SIZE);

BUILD_ASSERT(CONFIG_APP_MODULE_WATCHDOG_TIMEOUT_SECONDS > CONFIG_APP_MODULE_EXEC_TIME_SECONDS_MAX,
	     "Watchdog timeout must be greater than maximum execution time");

// TODO: configure simple_config, forward received led status to led module

int config_cb(const char *key, const struct simple_config_val *val)
{
	int err;

	if (val->type == SIMPLE_CONFIG_VAL_STRING) {
		LOG_DBG("\"%s\" = %s", key, val->val._str);
	} else if (val->type == SIMPLE_CONFIG_VAL_BOOL) {
		LOG_DBG("\"%s\" = %s", key, val->val._bool ? "true" : "false");
	} else if (val->type == SIMPLE_CONFIG_VAL_DOUBLE) {
		LOG_DBG("\"%s\" = %f", key, val->val._double);
	} else {
		return -EINVAL;
	}

	if (strcmp(key, "led_red") == 0) {
		int status = val->val._bool;

		err = zbus_chan_pub(&LED_CHAN, &status, K_NO_WAIT);
		if (err) {
			LOG_ERR("zbus_chan_pub, error:%d", err);
			SEND_FATAL_ERROR();
		}

		return 0;
	}

	return -EINVAL;
}

static void init_app_settings(void)
{
	struct simple_config_val val = {.type = SIMPLE_CONFIG_VAL_BOOL, .val._bool = true};
	int err = simple_config_set("led_red", &val);
	if (err) {
		LOG_ERR("simple_config_set, error: %d", err);
		SEND_FATAL_ERROR();
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

	LOG_DBG("Application module task started");

	task_wdt_id = task_wdt_add(wdt_timeout_ms, task_wdt_callback, (void *)k_current_get());

	/* Set up callback for runtime config changes from cloud */
	simple_config_set_callback(config_cb);

	/* Set initial settting values (can be skipped if cloud initializes shadow) */
	init_app_settings();

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

			if (cloud_status == CLOUD_CONNECTED) {
				LOG_INF("Cloud connected");
				LOG_INF("Getting latest device configuration from cloud");

				simple_config_update();
			} else {
				LOG_INF("Cloud disconnected");
			}
		}

		if ((&TRIGGER_CHAN == chan) && (cloud_status == CLOUD_CONNECTED)) {
			LOG_INF("Getting latest device configuration from cloud");

			simple_config_update();
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
