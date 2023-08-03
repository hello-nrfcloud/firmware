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
LOG_MODULE_REGISTER(led, CONFIG_APP_MODULE_LED_LOG_LEVEL);

const static struct device *red_led = DEVICE_DT_GET(DT_NODELABEL(red_led));
const static struct device *green_led = DEVICE_DT_GET(DT_NODELABEL(green_led));
const static struct device *blue_led = DEVICE_DT_GET(DT_NODELABEL(blue_led));

void led_callback(const struct zbus_channel *chan)
{
    int err = 0;
    const struct led_message *led_config;

    if (&LED_CHAN == chan) {
        // TODO
    }
}

ZBUS_LISTENER_DEFINE(led, led_callback);