/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(sampler, CONFIG_APP_MODULE_SAMPLER_LOG_LEVEL);

void sampler_callback(const struct zbus_channel *chan) {
	// TODO
}

/* Register listener */
ZBUS_LISTENER_DEFINE(sampler, sampler_callback);
