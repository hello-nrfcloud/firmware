/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <simple_config/simple_config.h>

#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(app, CONFIG_APP_LOG_LEVEL);

/* Register subscriber */
ZBUS_SUBSCRIBER_DEFINE(app, CONFIG_APP_MODULE_MESSAGE_QUEUE_SIZE);

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

static bool check_cloud_connection(void)
{
	enum cloud_status cloud_status;
	int err = zbus_chan_read(&CLOUD_CHAN, &cloud_status, K_FOREVER);

	if (err) {
		LOG_ERR("zbus_chan_read, error: %d", err);
		SEND_FATAL_ERROR();
	}
	return cloud_status == CLOUD_CONNECTED;
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

static void wait_for_cloud_connection(void)
{
	const struct zbus_channel *chan;

	while (!zbus_sub_wait(&app, &chan, K_FOREVER)) {
		if (&CLOUD_CHAN == chan) {
			if (check_cloud_connection()) {
				return;
			}
		}
	}
}

static void app_task(void)
{
	const struct zbus_channel *chan;

	/* Set up callback for runtime config changes from cloud */
	simple_config_set_callback(config_cb);

	/* Wait for cloud connection */
	wait_for_cloud_connection();

	/* Set initial settting values (can be skipped if cloud initializes shadow) */
	init_app_settings();

	/* Initial config update */
	simple_config_update();

	while (!zbus_sub_wait(&app, &chan, K_FOREVER)) {
		if (&CLOUD_CHAN == chan) {
			if (!check_cloud_connection()) {
				wait_for_cloud_connection();
			}
		} else if (&TRIGGER_CHAN == chan) {
			/* handle incoming cloud settings */
			simple_config_update();
		}
	}
}

K_THREAD_DEFINE(app_task_id,
		CONFIG_APP_MODULE_THREAD_STACK_SIZE,
		app_task, NULL, NULL, NULL, 3, 0, 0);
