/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <dk_buttons_and_leds.h>

#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(led, CONFIG_APP_LED_LOG_LEVEL);

void led_callback(const struct zbus_channel *chan)
{
	int err;
	const int *status;

	if (&LED_CHAN == chan) {
		/* Get LED status from channel. */
		status = zbus_chan_const_msg(chan);

		/* Blue LED */
		err = dk_set_led(DK_LED2, *status);
		if (err) {
			LOG_ERR("dk_set_led, error:%d", err);
			SEND_FATAL_ERROR();
			return;
		}
	}

	if (&FATAL_ERROR_CHAN == chan) {
		/* Red LED */
		err = dk_set_led_on(DK_LED1);
		if (err) {
			LOG_ERR("dk_set_led_on, error:%d", err);
			SEND_FATAL_ERROR();
			return;
		}
	}
}

/* Register listener - led_callback will be called everytime a channel that the module listens on
 * receives a new message.
 */
ZBUS_LISTENER_DEFINE(led, led_callback);

static int leds_init(void)
{
	__ASSERT((dk_leds_init() == 0), "DK LEDs init failure");

	return 0;
}

SYS_INIT(leds_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
