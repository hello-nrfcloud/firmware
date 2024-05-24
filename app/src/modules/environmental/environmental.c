/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/drivers/sensor.h>
#include <drivers/bme68x_iaq.h>
#include <date_time.h>

#include "message_channel.h"
#include "env_object_encode.h"

/* Register log module */
LOG_MODULE_REGISTER(env, CONFIG_APP_ENV_LOG_LEVEL);

static const struct device *const sensor_dev = DEVICE_DT_GET(DT_ALIAS(gas_sensor));

static struct payload payload = { 0 };
static void submit_payload_work_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(submit_payload_work, submit_payload_work_fn);

static void submit_payload_work_fn(struct k_work *work) {
	LOG_DBG("Submitting payload");
	int err = zbus_chan_pub(&PAYLOAD_CHAN, &payload, K_NO_WAIT);
	if (err == -EBUSY || err == -EAGAIN) {
		LOG_WRN("zbus_chan_pub, error: %d, retrying in a second...", err);
		k_work_reschedule(&submit_payload_work, K_SECONDS(1));
	} else if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
	} else {
		LOG_DBG("Payload sent");
	}
}

void env_callback(const struct zbus_channel *chan)
{
	struct sensor_value temp, press, humidity, iaq, co2, voc;

	struct env_object env_obj;
	int64_t system_time = k_uptime_get();
	int ret;

	if (&TRIGGER_CHAN == chan) {
		ret = sensor_sample_fetch(sensor_dev);
		__ASSERT_NO_MSG(ret == 0);
		ret = sensor_channel_get(sensor_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
		__ASSERT_NO_MSG(ret == 0);
		ret = sensor_channel_get(sensor_dev, SENSOR_CHAN_PRESS, &press);
		__ASSERT_NO_MSG(ret == 0);
		ret = sensor_channel_get(sensor_dev, SENSOR_CHAN_HUMIDITY, &humidity);
		__ASSERT_NO_MSG(ret == 0);
		ret = sensor_channel_get(sensor_dev, SENSOR_CHAN_CO2, &co2);
		__ASSERT_NO_MSG(ret == 0);
		ret = sensor_channel_get(sensor_dev, SENSOR_CHAN_VOC, &voc);
		__ASSERT_NO_MSG(ret == 0);
		ret = sensor_channel_get(sensor_dev, SENSOR_CHAN_IAQ, &iaq);
		__ASSERT_NO_MSG(ret == 0);

		LOG_DBG("temp: %d.%06d; press: %d.%06d; humidity: %d.%06d; iaq: %d; CO2: %d.%06d; "
			"VOC: %d.%06d",
			temp.val1, temp.val2, press.val1, press.val2, humidity.val1, humidity.val2,
			iaq.val1, co2.val1, co2.val2, voc.val1, voc.val2);

		ret = date_time_uptime_to_unix_time_ms(&system_time);
		if (ret) {
			LOG_ERR("Failed to convert uptime to unix time, error: %d", ret);
			return;
		}

		env_obj.temperature_m.bt = system_time;
		env_obj.temperature_m.vf = sensor_value_to_double(&temp);
		env_obj.humidity_m.vf = sensor_value_to_double(&humidity);
		env_obj.pressure_m.vf = sensor_value_to_double(&press) / 100;
		env_obj.iaq_m.vi = iaq.val1;

		ret = cbor_encode_env_object(payload.string, sizeof(payload.string),
				       &env_obj, &payload.string_len);
		if (ret) {
			LOG_ERR("Failed to encode env object, error: %d", ret);
			SEND_FATAL_ERROR();
			return;
		}

		ret = k_work_reschedule(&submit_payload_work, K_NO_WAIT);
		if (ret && ret != 1) {
			LOG_ERR("Failed to reschedule work, error: %d", ret);
			SEND_FATAL_ERROR();
		}
	}
}

ZBUS_LISTENER_DEFINE(env, env_callback);
