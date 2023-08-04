/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include "message_channel.h"
#include <date_time.h>

/* Register log module */
LOG_MODULE_REGISTER(sampler, CONFIG_APP_MODULE_SAMPLER_LOG_LEVEL);

struct deviceToCloud_message collected_msg;

static int64_t get_ts(void)
{
	int64_t ts;
	int err;

	err = date_time_now(&ts);
	if (err) {
		LOG_ERR("Error getting time: %d", err);
		ts = 0;
	}
	return ts;
}

void sampler_callback(const struct zbus_channel *chan) {
	    if (&TRIGGER_CHAN == chan) {
        // TODO
			collected_msg.union_count = 1;
			collected_msg._union[0].union_choice = _deviceToCloud_message_union__battery_message;
			collected_msg._union[0]._battery_message.battery_percentage = 73;
			collected_msg._union[0]._battery_message.timestamp = get_ts();
			int err = zbus_chan_pub(&MSG_OUT_CHAN, &collected_msg, K_SECONDS(10));
			if (err) {
				LOG_ERR("Could not publish message, err: %d", err);
			}
		}
}

/* Register listener */
ZBUS_LISTENER_DEFINE(sampler, sampler_callback);
