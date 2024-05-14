/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/drivers/sensor/npm1300_charger.h>
#include "nrf_fuel_gauge.h"
#include <math.h>
#include <zephyr/sys/util.h>

#include "lp803448_model.h"
#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(battery, CONFIG_APP_BATTERY_LOG_LEVEL);

/* Register subscriber */
ZBUS_SUBSCRIBER_DEFINE(battery, CONFIG_APP_BATTERY_MESSAGE_QUEUE_SIZE);

#define FG_BURST_COUNT 10
#define FG_BURST_DELAY K_MSEC(100)

static const struct device *charger = DEVICE_DT_GET(DT_NODELABEL(npm1300_charger));
static int64_t fg_ref_time;
static float max_charge_current;
static float term_charge_current;
static float fg_burst_current_accumulated = 0;
static float fg_burst_delta_accumulated = 0;
static int fg_burst_index = 0;

static int charger_read_sensors(float *voltage, float *current, float *temp)
{
	struct sensor_value value;
	int ret;

	ret = sensor_sample_fetch(charger);
	if (ret < 0) {
		return ret;
	}

	sensor_channel_get(charger, SENSOR_CHAN_GAUGE_VOLTAGE, &value);
	*voltage = (float)value.val1 + ((float)value.val2 / 1000000);

	sensor_channel_get(charger, SENSOR_CHAN_GAUGE_TEMP, &value);
	*temp = (float)value.val1 + ((float)value.val2 / 1000000);

	sensor_channel_get(charger, SENSOR_CHAN_GAUGE_AVG_CURRENT, &value);
	*current = (float)value.val1 + ((float)value.val2 / 1000000);

	return 0;
}

static void sample(void)
{
	/* Fuel gauge variables */
	float voltage;
	float current;
	float temp;
	float state_of_charge;
	float delta;
	float time_to_empty;
	float time_to_full;
	static struct nrf_fuel_gauge_state_info fg_state;
	int ret;

	/* Update fuel gauge state */
	ret = charger_read_sensors(&voltage, &current, &temp);
	if (ret) {
		LOG_ERR("Error: Could not read from charger device: %d", ret);
		SEND_FATAL_ERROR();
		return;
	}

	delta = (float) k_uptime_delta(&fg_ref_time) / 1000.f;
	fg_ref_time = k_uptime_get();
	state_of_charge = nrf_fuel_gauge_process(voltage, current, temp, delta, &fg_state);

	/* calculate TTE/TTF */
	time_to_empty = nrf_fuel_gauge_tte_get();
	time_to_full = nrf_fuel_gauge_ttf_get((bool)-max_charge_current, -term_charge_current);

	fg_burst_current_accumulated += delta * current;
	fg_burst_delta_accumulated += delta;

	/* Repeat readings upto FG_BURST_COUNT measurement until we collect enough data points */
	if (fg_burst_index++ < FG_BURST_COUNT) {
		k_sleep(FG_BURST_DELAY);
		sample();
		return;
	}

	current = fg_burst_current_accumulated / fg_burst_delta_accumulated;

	LOG_DBG("State of charge: %f", (double)roundf(state_of_charge));
	LOG_DBG("Voltage: %f", (double)roundf(1000 * voltage));
	LOG_DBG("Current: %f", (double)roundf(1000 * current));

	if (isnormal(time_to_full)) {
		LOG_DBG("Time to full: %f", (double)roundf(time_to_full));
	}

	if (isnormal(time_to_empty)) {
		LOG_DBG("Time to empty: %f", (double)roundf(time_to_empty));
	}

	/* Reset burst variables */
	fg_burst_index = 0;
	fg_burst_current_accumulated = 0;
	fg_burst_delta_accumulated = 0;

	/* TODO: Encode in CBOR and publish to PAYLOAD channel

		struct payload payload = { 0 };

		err = zbus_chan_pub(&PAYLOAD_CHAN, &payload, K_SECONDS(1));
		if (err) {
			LOG_ERR("zbus_chan_pub, error:%d", err);
			SEND_FATAL_ERROR();
		}
	*/
}

static void battery_task(void)
{
	const struct zbus_channel *chan;
	struct sensor_value value;
	struct nrf_fuel_gauge_init_parameters parameters = {
		.model = &battery_model
	};

	LOG_DBG("Battery module task started");

	if (!device_is_ready(charger)) {
		LOG_ERR("Charger device not ready.");
		SEND_FATAL_ERROR();
		return;
	}

	int err = charger_read_sensors(&parameters.v0, &parameters.i0, &parameters.t0);

	if (err < 0) {
		LOG_ERR("charger_read_sensors, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	nrf_fuel_gauge_init(&parameters, NULL);
	fg_ref_time = k_uptime_get();

	err = sensor_channel_get(charger, SENSOR_CHAN_GAUGE_DESIRED_CHARGING_CURRENT, &value);
	if (err) {
		LOG_ERR("sensor_channel_get(DESIRED_CHARGING_CURRENT), error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	max_charge_current = (float)value.val1 + ((float)value.val2 / 1000000);
	term_charge_current = max_charge_current / 10.f;

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
