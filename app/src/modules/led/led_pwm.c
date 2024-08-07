/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <string.h>

#include "led.h"
#include "led_pwm.h"
#include "led_effect.h"

LOG_MODULE_REGISTER(app_led_pwm, CONFIG_APP_LED_LOG_LEVEL);

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

struct led {
	size_t id;
	struct led_color color;
	const struct led_effect *effect;
	uint16_t effect_step;
	uint16_t effect_substep;

	struct k_work_delayable work;
	struct k_work_sync work_sync;
};

static const struct led_effect effect[] = {
	[LED_LTE_CONNECTING]	 = LED_EFFECT_LED_BREATHE(
					LED_ON_PERIOD_NORMAL,
					LED_OFF_PERIOD_NORMAL,
					LED_LTE_CONNECTING_COLOR
				   ),
	[LED_POLL_MODE]		 = LED_EFFECT_LED_BREATHE(
					LED_ON_PERIOD_SHORT,
					LED_OFF_PERIOD_LONG,
					LED_POLL_MODE_COLOR
				   ),
	[LED_GNSS_SEARCHING]	 = LED_EFFECT_LED_BREATHE(
					LED_ON_PERIOD_NORMAL,
					LED_OFF_PERIOD_NORMAL,
					LED_GNSS_SEARCHING_COLOR
				   ),
	[LED_ERROR_SYSTEM_FAULT] = LED_EFFECT_LED_BREATHE(
					LED_ON_PERIOD_ERROR,
					LED_OFF_PERIOD_ERROR,
					LED_ERROR_SYSTEM_FAULT_COLOR
				   ),
	[LED_ERROR_IRRECOVERABLE] = LED_EFFECT_LED_BREATHE(
					LED_ON_PERIOD_SHORT,
					LED_OFF_PERIOD_LONG,
					LED_ERROR_SYSTEM_FAULT_COLOR
				   ),
	[LED_OFF]		 = LED_EFFECT_LED_BREATHE(
					LED_ON_PERIOD_ERROR,
					LED_OFF_PERIOD_ERROR,
					LED_OFF_COLOR
				   ),
};

static struct led leds;

static void pwm_out(const struct led_color *color)
{
	int err;

	/* RED */
	err = pwm_set_dt(&led0, PWM_USEC(255), PWM_USEC(color->c[0]));
	if (err) {
		LOG_ERR("pwm_set_dt, error:%d", err);
		return;
	}

	/* GREEN */
	err = pwm_set_dt(&led2, PWM_USEC(255), PWM_USEC(color->c[1]));
	if (err) {
		LOG_ERR("pwm_set_dt, error:%d", err);
		return;
	}

	/* BLUE */
	err = pwm_set_dt(&led1, PWM_USEC(255), PWM_USEC(color->c[2]));
	if (err) {
		LOG_ERR("pwm_set_dt, error:%d", err);
		return;
	}
}

static void work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	const struct led_effect_step *effect_step =
		&leds.effect->steps[leds.effect_step];
	int substeps_left = effect_step->substep_cnt - leds.effect_substep;

	for (size_t i = 0; i < ARRAY_SIZE(leds.color.c); i++) {
		int diff = (effect_step->color.c[i] - leds.color.c[i]) /
			   substeps_left;
		leds.color.c[i] += diff;
	}

	pwm_out(&leds.color);

	leds.effect_substep++;
	if (leds.effect_substep == effect_step->substep_cnt) {
		leds.effect_substep = 0;
		leds.effect_step++;

		if (leds.effect_step == leds.effect->step_cnt) {
			if (leds.effect->loop_forever) {
				leds.effect_step = 0;
			}
		} else {
			__ASSERT_NO_MSG(leds.effect->steps[leds.effect_step]
						.substep_cnt > 0);
		}
	}

	if (leds.effect_step < leds.effect->step_cnt) {
		int32_t next_delay =
			leds.effect->steps[leds.effect_step].substep_time;

		k_work_reschedule(&leds.work, K_MSEC(next_delay));
	}
}

static void led_update(struct led *led)
{
	k_work_cancel_delayable_sync(&led->work, &led->work_sync);

	led->effect_step = 0;
	led->effect_substep = 0;

	if (!led->effect) {
		LOG_DBG("No effect set");
		return;
	}

	__ASSERT_NO_MSG(led->effect->steps);

	if (led->effect->step_cnt > 0) {
		int32_t next_delay =
			led->effect->steps[led->effect_step].substep_time;

		k_work_schedule(&led->work, K_MSEC(next_delay));
	} else {
		LOG_DBG("LED effect with no effect");
	}
}

void led_pwm_start(void)
{
	int err;

	// this action starts also led1 and led2 pwm
	err = pm_device_action_run(led0.dev, PM_DEVICE_ACTION_RESUME);
	if (err) {
		LOG_ERR("PWM enable failed");
		return;
	}
}

void led_pwm_stop(void)
{
	int err;
	k_work_cancel_delayable_sync(&leds.work, &leds.work_sync);

	// this action suspends also led1 and led2 pwm
	err = pm_device_action_run(led0.dev, PM_DEVICE_ACTION_SUSPEND);
	if (err) {
		LOG_ERR("PWM disable failed");
		return;
	}
}

void led_pwm_set_effect(enum led_state state)
{
	int err;
	enum pm_device_state led0_pwm_power_state;

	if (pwm_is_ready_dt(&led0)) {
		err = pm_device_state_get(led0.dev, &led0_pwm_power_state);
		if (err) {
			LOG_ERR("Failed to assess leds pwm power state, pm_device_state_get: %d.",
				err);
			return;
		}

		if (state == LED_OFF) {
			// stop leds pwm if on, and return
			if (led0_pwm_power_state == PM_DEVICE_STATE_ACTIVE) {
				led_pwm_stop();
			}
			return;
		}

		if (led0_pwm_power_state == PM_DEVICE_STATE_SUSPENDED) {
			// start leds pwm if off
			led_pwm_start();
		}
	}

	leds.effect = &effect[state];
	led_update(&leds);
}

static struct led_effect effect_on = LED_EFFECT_LED_ON(LED_NOCOLOR());

int led_pwm_set_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
	int err;
	enum pm_device_state led0_pwm_power_state;

	if (pwm_is_ready_dt(&led0)) {
		err = pm_device_state_get(led0.dev, &led0_pwm_power_state);
		if (err) {
			LOG_ERR("Failed to assess leds pwm power state, pm_device_state_get: %d.",
				err);
			return 0;
		}

        if (red == 0 && green == 0 && blue == 0) {
			// stop leds pwm if on, and return
			if (led0_pwm_power_state == PM_DEVICE_STATE_ACTIVE) {
				led_pwm_stop();
			}
			return 0;
		}

		if (led0_pwm_power_state == PM_DEVICE_STATE_SUSPENDED) {
			// start leds pwm if off
			led_pwm_start();
		}
	}

	effect_on.steps[0].color.c[0] = red;
	effect_on.steps[0].color.c[1] = green;
	effect_on.steps[0].color.c[2] = blue;

	leds.effect = &effect_on;
	led_update(&leds);

	return 0;
}

static int led_pwm_init(void)
{
	k_work_init_delayable(&leds.work, work_handler);

	return 0;
}

SYS_INIT(led_pwm_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
