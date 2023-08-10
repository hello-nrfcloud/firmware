/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/drivers/led.h>
#include <zephyr/device.h>

#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(led, CONFIG_APP_MODULE_LED_LOG_LEVEL);

#define NUM_CHANNELS 3

const static struct device *led_device = DEVICE_DT_GET_ANY(pwm_leds);

void led_callback(const struct zbus_channel *chan)
{
    int err = 0;
    struct led_message led_config;

    if (&LED_CHAN == chan) {
        if (!device_is_ready(led_device)) {
            LOG_ERR("LED device is not ready!");
            return;
        }
        err = zbus_chan_read(&LED_CHAN, &led_config, K_SECONDS(1));
        if (err) {
            LOG_ERR("zbus_chan_read, error: %d", err);
            SEND_FATAL_ERROR();
            return;
        }
        uint8_t colors[NUM_CHANNELS] = {
            (uint8_t) led_config.led_red,
            (uint8_t) led_config.led_green,
            (uint8_t) led_config.led_blue
        };
        for (size_t i = 0; i < NUM_CHANNELS; ++i) {
            led_set_brightness(led_device, i, colors[i]);
        }
    }
}

ZBUS_LISTENER_DEFINE(led, led_callback);
