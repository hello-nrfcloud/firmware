/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include "message_channel.h"

LOG_MODULE_REGISTER(movement, CONFIG_APP_MODULE_MOVEMENT_LOG_LEVEL);


static void trigger_callback(const struct zbus_channel *chan)
{
    int err = 0;

    if (&TRIGGER_CHAN == chan) {
        // TODO
    }
}

ZBUS_LISTENER_DEFINE(movement, trigger_callback);