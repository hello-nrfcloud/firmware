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
#include "led_pwm.h"
#include "led.h"

/* Register log module */
LOG_MODULE_REGISTER(led, CONFIG_APP_LED_LOG_LEVEL);

void led_callback(const struct zbus_channel *chan);

/* Register listener - led_callback will be called everytime a channel that the module listens on
 * receives a new message.
 */
ZBUS_LISTENER_DEFINE(led, led_callback);

/* Observe channels */
ZBUS_CHAN_ADD_OBS(ERROR_CHAN, led, 0);
ZBUS_CHAN_ADD_OBS(CONFIG_CHAN, led, 0);
ZBUS_CHAN_ADD_OBS(NETWORK_CHAN, led, 0);
ZBUS_CHAN_ADD_OBS(TRIGGER_MODE_CHAN, led, 0);

/* Forward declarations */
static void led_pattern_update_work_fn(struct k_work *work);

/* Definition used to specify LED patterns that should hold forever. */
#define HOLD_FOREVER -1

/* List of LED patterns supported in the UI module. */
static struct led_pattern {
	/* Variable used to construct a linked list of led patterns. */
	sys_snode_t header;
	/* LED state. */
	enum led_state led_state;
	/* Duration of the LED state. */
	int16_t duration_sec;

	/* RGB values for the LED state. */
	uint8_t red;
	uint8_t green;
	uint8_t blue;

} led_pattern_list[LED_PATTERN_COUNT];

/* Linked list used to schedule multiple LED pattern transitions. */
static sys_slist_t pattern_transition_list = SYS_SLIST_STATIC_INIT(&pattern_transition_list);

/* Delayed work that is used to display and transition to the correct LED pattern depending on the
 * internal state of the module.
 */
static K_WORK_DELAYABLE_DEFINE(led_pattern_update_work, led_pattern_update_work_fn);

/* Function that clears LED pattern transition list. */
static void transition_list_clear(void)
{
	struct led_pattern *transition = NULL;
	struct led_pattern *next_transition = NULL;

	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&pattern_transition_list,
					  transition,
					  next_transition,
					  header) {
		sys_slist_remove(&pattern_transition_list, NULL, &transition->header);
	};
}

/* Function that appends a LED state and a corresponding duration to the
 * LED pattern transition list. If the LED state is set to LED_CONFIGURED, the RGB values are
 * also appended to the list and used to set the desired color.
 */
static void transition_list_append(enum led_state led_state, int16_t duration_sec,
				   uint8_t red, uint8_t green, uint8_t blue)
{
	led_pattern_list[led_state].led_state = led_state;
	led_pattern_list[led_state].duration_sec = duration_sec;

	if (led_state == LED_CONFIGURED) {
		led_pattern_list[led_state].red = red;
		led_pattern_list[led_state].green = green;
		led_pattern_list[led_state].blue = blue;
	}

	sys_slist_append(&pattern_transition_list, &led_pattern_list[led_state].header);
}

static void led_pattern_update_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);

	struct led_pattern *next_pattern;
	static enum led_state previous_led_state = LED_PATTERN_COUNT;
	sys_snode_t *node = sys_slist_get(&pattern_transition_list);

	if (node == NULL) {
		LOG_ERR("Cannot find any more LED pattern transitions");
		return;
	}

	next_pattern = CONTAINER_OF(node, struct led_pattern, header);

	/* Prevent the same LED led_state from being scheduled twice in a row. */
	if (next_pattern->led_state != previous_led_state) {

		if (next_pattern->led_state == LED_CONFIGURED) {
			led_pwm_set_rgb(next_pattern->red, next_pattern->green, next_pattern->blue);
		} else {
			led_pwm_set_effect(next_pattern->led_state);
		}

		previous_led_state = next_pattern->led_state;
	}

	/* Even if the LED state is not updated due a match with the previous state a LED pattern
	 * update is scheduled. This will prolong the pattern until the LED pattern transition
	 * list is cleared.
	 */
	if (next_pattern->duration_sec > 0) {
		k_work_reschedule(&led_pattern_update_work, K_SECONDS(next_pattern->duration_sec));
	}
}

void led_callback(const struct zbus_channel *chan)
{
	if (&TRIGGER_MODE_CHAN == chan) {
		const enum trigger_mode *mode = zbus_chan_const_msg(chan);

		if (*mode == TRIGGER_MODE_NORMAL) {
			LOG_DBG("Trigger mode normal");

			/* Clear all LED patterns and stop the PWM.
			 * I think we need to use the LED pwm API directly in some way here
			 * to ensure that we are disabling the PWM.
			 */
			transition_list_clear();
			transition_list_append(LED_OFF, HOLD_FOREVER, 0, 0, 0);

			k_work_reschedule(&led_pattern_update_work, K_NO_WAIT);

		} else if (*mode == TRIGGER_MODE_POLL) {
			LOG_DBG("Trigger mode poll");

			transition_list_clear();

			/* If we enter poll mode and the LED is configured, we set the configured
			 * color.
			 */
			if (led_pattern_list[LED_CONFIGURED].red != 0 ||
			    led_pattern_list[LED_CONFIGURED].green != 0 ||
			    led_pattern_list[LED_CONFIGURED].blue != 0) {

				transition_list_append(LED_CONFIGURED, HOLD_FOREVER,
						       led_pattern_list[LED_CONFIGURED].red,
						       led_pattern_list[LED_CONFIGURED].green,
						       led_pattern_list[LED_CONFIGURED].blue);
			} else {
				transition_list_append(LED_POLL_MODE, HOLD_FOREVER, 0, 0, 0);
			}

			k_work_reschedule(&led_pattern_update_work, K_NO_WAIT);
		}
	}

	if (&NETWORK_CHAN == chan) {

		const enum network_status *status = zbus_chan_const_msg(chan);

		if (*status == NETWORK_DISCONNECTED) {
			LOG_DBG("Network disconnected");

			transition_list_clear();
			transition_list_append(LED_LTE_CONNECTING, HOLD_FOREVER, 0, 0, 0);

			k_work_reschedule(&led_pattern_update_work, K_NO_WAIT);
		}
	}

	if (&CONFIG_CHAN == chan) {
		/* Get LED configuration from channel. */

		const struct configuration *config = zbus_chan_const_msg(chan);

		if (config->led_present == false) {
			LOG_DBG("LED configuration not present");
			return;
		}

		LOG_DBG("LED configuration: red:%d, green:%d, blue:%d",
			config->led_red, config->led_green, config->led_blue);

		transition_list_clear();
		transition_list_append(LED_CONFIGURED, HOLD_FOREVER,
				       (uint8_t)config->led_red,
				       (uint8_t)config->led_green,
				       (uint8_t)config->led_blue);

		k_work_reschedule(&led_pattern_update_work, K_NO_WAIT);
	}

	if (&ERROR_CHAN == chan) {
		const enum error_type *type = zbus_chan_const_msg(chan);

		if (*type == ERROR_FATAL) {
			transition_list_clear();
			transition_list_append(LED_ERROR_SYSTEM_FAULT, HOLD_FOREVER, 0, 0, 0);
		}
	}
}

static int leds_init(void)
{
	__ASSERT((dk_leds_init() == 0), "DK LEDs init failure");

	return 0;
}

SYS_INIT(leds_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
