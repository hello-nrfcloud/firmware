/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/drivers/led.h>

#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(led, CONFIG_APP_LED_LOG_LEVEL);

const static struct device *led_device = DEVICE_DT_GET_ANY(gpio_leds);

/* LED 1, green on Thingy:91 boards. */
#define LED_1_GREEN 0

void led_callback(const struct zbus_channel *chan)
{
	int err = 0;
	const int *status;

	if (&LED_CHAN == chan) {

		if (!device_is_ready(led_device)) {
			LOG_ERR("LED device is not ready");
			return;
		}

		/* Get network status from channel. */
		status = zbus_chan_const_msg(chan);

		if (*status) {
			err = led_on(led_device, LED_1_GREEN);
			if (err) {
				LOG_ERR("led_on, error: %d", err);
			}
		} else {
			err = led_off(led_device, LED_1_GREEN);
			if (err) {
				LOG_ERR("led_off, error: %d", err);
			}
		}
	}
}

/* Register listener - led_callback will be called everytime a channel that the module listens on
 * receives a new message.
 */
ZBUS_LISTENER_DEFINE(led, led_callback);
