/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/drivers/sensor/npm1300_charger.h>
#include <zephyr/sys/util.h>
#include <nrf_fuel_gauge.h>
#include <date_time.h>
#include <math.h>

#include "lp803448_model.h"
#include "message_channel.h"
#include "bat_object_encode.h"

/* Register log module */
LOG_MODULE_REGISTER(battery, CONFIG_APP_BATTERY_LOG_LEVEL);

/* Register subscriber */
ZBUS_SUBSCRIBER_DEFINE(battery, CONFIG_APP_BATTERY_MESSAGE_QUEUE_SIZE);

/* nPM1300 register bitmasks */

/* CHARGER.BCHGCHARGESTATUS.TRICKLECHARGE */
#define NPM1300_CHG_STATUS_TC_MASK BIT(2)
/* CHARGER.BCHGCHARGESTATUS.CONSTANTCURRENT */
#define NPM1300_CHG_STATUS_CC_MASK BIT(3)
/* CHARGER.BCHGCHARGESTATUS.CONSTANTVOLTAGE */
#define NPM1300_CHG_STATUS_CV_MASK BIT(4)

static const struct device *charger = DEVICE_DT_GET(DT_NODELABEL(npm1300_charger));
static int64_t fg_ref_time;

static int charger_read_sensors(float *voltage, float *current, float *temp, int32_t *chg_status)
{
	struct sensor_value value;
	int err;

	err = sensor_sample_fetch(charger);
	if (err < 0) {
		return err;
	}

	sensor_channel_get(charger, SENSOR_CHAN_GAUGE_VOLTAGE, &value);
	*voltage = (float)value.val1 + ((float)value.val2 / 1000000);

	sensor_channel_get(charger, SENSOR_CHAN_GAUGE_TEMP, &value);
	*temp = (float)value.val1 + ((float)value.val2 / 1000000);

	sensor_channel_get(charger, SENSOR_CHAN_GAUGE_AVG_CURRENT, &value);
	*current = (float)value.val1 + ((float)value.val2 / 1000000);

	sensor_channel_get(charger, (enum sensor_channel)SENSOR_CHAN_NPM1300_CHARGER_STATUS,
			   &value);
	*chg_status = value.val1;

	return 0;
}

static void sample(void)
{
	int err;
	int chg_status;
	bool charging;
	float voltage;
	float current;
	float temp;
	float state_of_charge;
	float delta;
	struct bat_object bat_object = { 0 };
	struct payload payload = { 0 };
	int64_t system_time = k_uptime_get();

	err = date_time_uptime_to_unix_time_ms(&system_time);
	if (err) {
		LOG_ERR("Failed to convert uptime to unix time, error: %d", err);
		return;
	}

	err = charger_read_sensors(&voltage, &current, &temp, &chg_status);
	if (err) {
		LOG_ERR("charger_read_sensors, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	delta = (float) k_uptime_delta(&fg_ref_time) / 1000.f;

	charging = (chg_status & (NPM1300_CHG_STATUS_TC_MASK |
				  NPM1300_CHG_STATUS_CC_MASK |
				  NPM1300_CHG_STATUS_CV_MASK)) != 0;

	state_of_charge = nrf_fuel_gauge_process(voltage, current, temp, delta, NULL);

	LOG_DBG("State of charge: %f", (double)roundf(state_of_charge));
	LOG_DBG("The battery is %s", charging ? "charging" : "not charging");

	bat_object.state_of_charge_m.bt = system_time;
	bat_object.state_of_charge_m.vi = (int32_t)(state_of_charge + 0.5f);
	bat_object.voltage_m.vf = voltage;
	bat_object.temperature_m.vf = temp;

	err = cbor_encode_bat_object(payload.string, sizeof(payload.string),
				     &bat_object, &payload.string_len);
	if (err) {
		LOG_ERR("Failed to encode env object, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	err = zbus_chan_pub(&PAYLOAD_CHAN, &payload, K_FOREVER);
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}
}

static void battery_task(void)
{
	int err;
	const struct zbus_channel *chan;
	struct sensor_value value;
	struct nrf_fuel_gauge_init_parameters parameters = {
		.model = &battery_model
	};
	int32_t chg_status;

	LOG_DBG("Battery module task started");

	if (!device_is_ready(charger)) {
		LOG_ERR("Charger device not ready.");
		SEND_FATAL_ERROR();
		return;
	}

	err = charger_read_sensors(&parameters.v0, &parameters.i0, &parameters.t0, &chg_status);
	if (err < 0) {
		LOG_ERR("charger_read_sensors, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	err = nrf_fuel_gauge_init(&parameters, NULL);
	if (err) {
		LOG_ERR("nrf_fuel_gauge_init, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	fg_ref_time = k_uptime_get();

	err = sensor_channel_get(charger, SENSOR_CHAN_GAUGE_DESIRED_CHARGING_CURRENT, &value);
	if (err) {
		LOG_ERR("sensor_channel_get(DESIRED_CHARGING_CURRENT), error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	while (!zbus_sub_wait(&battery, &chan, K_FOREVER)) {
		if (&TRIGGER_CHAN == chan) {
			LOG_DBG("Trigger received");
			sample();
		}
	}
}

K_THREAD_DEFINE(battery_task_id,
		CONFIG_APP_BATTERY_THREAD_STACK_SIZE,
		battery_task, NULL, NULL, NULL, 3, 0, 0);
