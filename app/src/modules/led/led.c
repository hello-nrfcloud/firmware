/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/pwm.h>
#include <dk_buttons_and_leds.h>

#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(led, CONFIG_APP_LED_LOG_LEVEL);

#define PWM_LED0_NODE	DT_ALIAS(pwm_led0)
#define PWM_LED1_NODE	DT_ALIAS(pwm_led1)
#define PWM_LED2_NODE	DT_ALIAS(pwm_led2)

#if DT_NODE_HAS_STATUS(PWM_LED0_NODE, okay)
static const struct pwm_dt_spec led0 = PWM_DT_SPEC_GET(PWM_LED0_NODE);
#else
#error "Unsupported board: pwm-led 0 devicetree alias is not defined"
#endif
#if DT_NODE_HAS_STATUS(PWM_LED1_NODE, okay)
static const struct pwm_dt_spec led1 = PWM_DT_SPEC_GET(PWM_LED1_NODE);
#else
#error "Unsupported board: pwm-led 1 devicetree alias is not defined"
#endif
#if DT_NODE_HAS_STATUS(PWM_LED2_NODE, okay)
static const struct pwm_dt_spec led2 = PWM_DT_SPEC_GET(PWM_LED2_NODE);
#else
#error "Unsupported board: pwm-led 2 devicetree alias is not defined"
#endif

#define PWM_PERIOD	20000U
#define LIGHTNESS_MAX	UINT8_MAX

void led_callback(const struct zbus_channel *chan)
{
	int err;

	if (&CONFIG_CHAN == chan) {
		/* Get LED configuration from channel. */

		const struct configuration *config = zbus_chan_const_msg(chan);

		LOG_DBG("LED configuration: red:%d, green:%d, blue:%d",
			config->led_red, config->led_green, config->led_blue);

		/* Red LED */
		err = pwm_set_dt(&led0, PWM_USEC(PWM_PERIOD),
				 PWM_USEC((PWM_PERIOD * config->led_red) / LIGHTNESS_MAX));
		if (err) {
			LOG_ERR("pwm_set_dt, error:%d", err);
			SEND_FATAL_ERROR();
			return;
		}

		/* Blue LED */
		err = pwm_set_dt(&led1, PWM_USEC(PWM_PERIOD),
				 PWM_USEC((PWM_PERIOD * config->led_blue) / LIGHTNESS_MAX));
		if (err) {
			LOG_ERR("pwm_set_dt, error:%d", err);
			SEND_FATAL_ERROR();
			return;
		}

		/* Green LED */
		err = pwm_set_dt(&led2, PWM_USEC(PWM_PERIOD),
				 PWM_USEC((PWM_PERIOD * config->led_green) / LIGHTNESS_MAX));
		if (err) {
			LOG_ERR("pwm_set_dt, error:%d", err);
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
