/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/drivers/sensor.h>
#if defined(CONFIG_DK_LIBRARY)
#include <dk_buttons_and_leds.h>
#endif /* defined(CONFIG_DK_LIBRARY) */

#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(trigger, CONFIG_APP_TRIGGER_LOG_LEVEL);

static void message_send(void)
{
	int not_used = -1;
	int err;
	bool fota_ongoing = true;

	err = zbus_chan_read(&FOTA_ONGOING_CHAN, &fota_ongoing, K_NO_WAIT);
	if (err) {
		LOG_ERR("zbus_chan_read, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	if (fota_ongoing) {
		LOG_DBG("FOTA ongoing, skipping trigger message");
		return;
	}

	LOG_DBG("Sending trigger message");
	err = zbus_chan_pub(&TRIGGER_CHAN, &not_used, K_SECONDS(1));
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
	}
}

#if defined(CONFIG_ADXL367)
static const struct device *accel_dev = DEVICE_DT_GET(DT_ALIAS(motion_sensor));
static const enum sensor_channel accel_channel = SENSOR_CHAN_ACCEL_X;
static const struct sensor_trigger accel_lp_sensor_trigger_motion = {
	.chan = SENSOR_CHAN_ACCEL_XYZ,
	.type = SENSOR_TRIG_THRESHOLD
};
#define ACCEL_LP_THRESHOLD_RESOLUTION_MAX 8000
#define ACCEL_LP_RANGE_MAX_M_S2 19.6133

static int motion_sensor_threshold_set(double threshold)
{
	int err, threshold_int;

	if ((threshold > ACCEL_LP_RANGE_MAX_M_S2) || (threshold <= 0.0)) {
		LOG_ERR("Invalid activity threshold value: %f", threshold);
		return -EINVAL;
	}

	LOG_DBG("threshold: %f", threshold);

	/* Scale to sensor setting value using ranges */
	threshold *= (ACCEL_LP_THRESHOLD_RESOLUTION_MAX / ACCEL_LP_RANGE_MAX_M_S2);
	LOG_DBG("scaled threshold: %f", threshold);

	/* Round to nearest integer */
	threshold_int = (int)(threshold + 0.5);
	LOG_DBG("rounded threshold: %d", threshold_int);

	/* Clamp to valid sensor setting value */
	threshold_int = CLAMP(threshold_int, 0, ACCEL_LP_THRESHOLD_RESOLUTION_MAX - 1);
	LOG_DBG("clamped threshold: %d", threshold_int);

	const struct sensor_value data = {
		.val1 = threshold_int
	};

	err = sensor_attr_set(accel_dev, accel_channel,	SENSOR_ATTR_UPPER_THRESH, &data);
	if (err) {
		LOG_ERR("Failed to set accelerometer threshold value, device: %s, error: %d",
			accel_dev->name, err);
		return err;
	}
	return 0;
}

static void motion_sensor_trigger_handler(const struct device *dev,
					  const struct sensor_trigger *trig)
{
	LOG_DBG("Motion trigger: %d", trig->type);
	if (trig->type == SENSOR_TRIG_THRESHOLD) {
		message_send();
	}
}

static int motion_sensor_trigger_callback_set(bool enable)
{
	int err = 0;

	sensor_trigger_handler_t handler = enable ? motion_sensor_trigger_handler : NULL;

	err = sensor_trigger_set(accel_dev, &accel_lp_sensor_trigger_motion, handler);
	if (err) {
		LOG_ERR("Could not set trigger for device %s, error: %d",
			accel_dev->name, err);
		return err;
	}
	return 0;
}

static void motion_sensor_init(void)
{
	int err;

	err = motion_sensor_threshold_set(CONFIG_APP_TRIGGER_MOTION_THRESHOLD_M_S2);
	if (err) {
		LOG_ERR("motion_sensor_threshold_set, error: %d", err);
		return;
	}

	err = motion_sensor_trigger_callback_set(true);
	if (err) {
		LOG_ERR("motion_sensor_trigger_callback_set, error: %d", err);
		return;
	}
}
#endif /* defined(CONFIG_ADXL367) */

#if defined(CONFIG_DK_LIBRARY)
static void button_handler(uint32_t button_states, uint32_t has_changed)
{
	if (has_changed & button_states) {
		message_send();
	}
}
#endif /* defined(CONFIG_DK_LIBRARY) */

static void trigger_task(void)
{
#if defined(CONFIG_DK_LIBRARY)
	int err = dk_buttons_init(button_handler);

	if (err) {
		LOG_ERR("dk_buttons_init, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}
#endif /* defined(CONFIG_DK_LIBRARY) */
#if defined(CONFIG_ADXL367)
	motion_sensor_init();
#endif /* defined(CONFIG_ADXL367) */
	while (true) {
		message_send();
		k_sleep(K_SECONDS(CONFIG_APP_TRIGGER_TIMEOUT_SECONDS));
	}
}

K_THREAD_DEFINE(trigger_task_id,
		CONFIG_APP_TRIGGER_THREAD_STACK_SIZE,
		trigger_task, NULL, NULL, NULL, 3, 0, 0);
