/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/task_wdt/task_wdt.h>
#include <dk_buttons_and_leds.h>

#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(trigger, CONFIG_APP_TRIGGER_LOG_LEVEL);

#define MSG_SEND_TIMEOUT_SECONDS 1

/* Forward declarations */
static void trigger_work_fn(struct k_work *work);

/* Delayable work used to schedule triggers. */
static K_WORK_DELAYABLE_DEFINE(trigger_work, trigger_work_fn);

/* Set default update interval */
k_timeout_t update_interval = K_SECONDS(CONFIG_APP_TRIGGER_TIMEOUT_SECONDS);

static void trigger_work_fn(struct k_work *work)
{
	int err;
	int not_used = -1;
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
		return;
	}

	k_work_reschedule(&trigger_work, update_interval);
}

static void button_handler(uint32_t button_states, uint32_t has_changed)
{
	if (has_changed & button_states) {
		trigger_work_fn(NULL);
	}
}

void trigger_callback(const struct zbus_channel *chan)
{
	if (&CONFIG_CHAN == chan) {
		/* Get update interval configuration from channel. */
		const struct configuration *config = zbus_chan_const_msg(chan);

		LOG_DBG("New update interval: %lld", config->update_interval);

		update_interval = K_SECONDS(config->update_interval);

		/* Reschedule work */
		k_work_reschedule(&trigger_work, update_interval);
	}

	if (&CLOUD_CHAN == chan) {
		LOG_DBG("Cloud connection status received");

		const enum cloud_status *status = zbus_chan_const_msg(chan);

		if (*status == CLOUD_CONNECTED) {
			LOG_DBG("Cloud connected, starting trigger");
			k_work_reschedule(&trigger_work, update_interval);
		} else {
			LOG_DBG("Cloud disconnected, stopping trigger");
			k_work_cancel_delayable(&trigger_work);
		}
	}
}

ZBUS_LISTENER_DEFINE(trigger, trigger_callback);

static int trigger_init(void)
{
	__ASSERT((dk_buttons_init(button_handler) == 0), "Task watchdog init failure");

	return 0;
}

SYS_INIT(trigger_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
