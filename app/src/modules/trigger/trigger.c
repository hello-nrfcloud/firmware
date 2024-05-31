/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/task_wdt/task_wdt.h>
#if CONFIG_DK_LIBRARY
#include <dk_buttons_and_leds.h>
#endif /* CONFIG_DK_LIBRARY */

#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(trigger, CONFIG_APP_TRIGGER_LOG_LEVEL);

#define MSG_SEND_TIMEOUT_SECONDS 1
BUILD_ASSERT(CONFIG_APP_TRIGGER_WATCHDOG_TIMEOUT_SECONDS >
			 (CONFIG_APP_TRIGGER_TIMEOUT_SECONDS + MSG_SEND_TIMEOUT_SECONDS),
			 "Watchdog timeout must be greater than maximum execution time");


static void task_wdt_callback(int channel_id, void *user_data)
{
	LOG_ERR("Watchdog expired, Channel: %d, Thread: %s",
		channel_id, k_thread_name_get((k_tid_t)user_data));

	SEND_FATAL_ERROR();
}

static void message_send(void)
{
	int not_used = -1;
	int err;
	bool fota_ongoing = true;

	err = zbus_chan_read(&FOTA_ONGOING_CHAN, &fota_ongoing, K_NO_WAIT);
	if (err) {
		LOG_ERR("zbus_chan_read, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	if (fota_ongoing) {
		LOG_DBG("FOTA ongoing, skipping trigger message");
		return;
	}

	LOG_DBG("Sending trigger message");
	err = zbus_chan_pub(&TRIGGER_CHAN, &not_used, K_SECONDS(MSG_SEND_TIMEOUT_SECONDS));
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
	}
}

#if CONFIG_DK_LIBRARY
static void button_handler(uint32_t button_states, uint32_t has_changed)
{
	if (has_changed & button_states) {
		message_send();
	}
}
#endif /* CONFIG_DK_LIBRARY */

static void trigger_task(void)
{
	int task_wdt_id;
	const uint32_t wdt_timeout_ms = (CONFIG_APP_TRIGGER_WATCHDOG_TIMEOUT_SECONDS * MSEC_PER_SEC);

	task_wdt_id = task_wdt_add(wdt_timeout_ms, task_wdt_callback, k_current_get());

#if CONFIG_DK_LIBRARY
	int err = dk_buttons_init(button_handler);

	if (err) {
		LOG_ERR("dk_buttons_init, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}
#endif /* CONFIG_DK_LIBRARY */

	while (true) {
		err = task_wdt_feed(task_wdt_id);
		if (err) {
			LOG_ERR("task_wdt_feed, error: %d", err);
			SEND_FATAL_ERROR();
			return;
		}

		message_send();
		k_sleep(K_SECONDS(CONFIG_APP_TRIGGER_TIMEOUT_SECONDS));
	}
}

K_THREAD_DEFINE(trigger_task_id,
		CONFIG_APP_TRIGGER_THREAD_STACK_SIZE,
		trigger_task, NULL, NULL, NULL, 3, 0, 0);
