/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/* This PWM driver is purele for testing and merely prints the values that were set. */

#define DT_DRV_COMPAT nordic_pwm_logger

#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pwm_logger, CONFIG_PWM_LOG_LEVEL);

__weak int nordic_pwm_logger_init(const struct device *dev)
{
	LOG_DBG("%s: Initialization complete", dev->name);
	return 0;
}

__weak int nordic_pwm_logger_set_cycles(const struct device *dev, uint32_t channel,
				 uint32_t period_cycles, uint32_t pulse_cycles,
				 pwm_flags_t flags)
{
	const char *polarity = (flags & PWM_POLARITY_INVERTED) ? "INVERTED" : "NORMAL";

	LOG_DBG("%s: Setting period=%d, pulse=%d, polarity=%s on channel %d",
		dev->name, period_cycles, pulse_cycles, polarity, channel);

	return 0;
}

__weak int nordic_pwm_logger_get_cycles_per_sec(const struct device *dev, uint32_t channel,
					 uint64_t *cycles)
{
	/* return 16MHz, like pwm_nrfx does. */
	*cycles = 16ul * 1000ul * 1000ul;
	return 0;
}

static const struct pwm_driver_api nordic_pwm_logger_api = {
    .set_cycles = nordic_pwm_logger_set_cycles,
    .get_cycles_per_sec = nordic_pwm_logger_get_cycles_per_sec,
};

#define PWM_LOGGER_INIT(n)                                                               \
	DEVICE_DT_INST_DEFINE(n, &nordic_pwm_logger_init, NULL, NULL, NULL, POST_KERNEL, \
			      CONFIG_PWM_INIT_PRIORITY, &nordic_pwm_logger_api);

DT_INST_FOREACH_STATUS_OKAY(PWM_LOGGER_INIT)
