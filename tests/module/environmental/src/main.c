/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <unity.h>

#include <zephyr/fff.h>
#include "message_channel.h"
#include "gas_sensor.h"
#include <zephyr/task_wdt/task_wdt.h>
#include <zephyr/logging/log.h>

DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(int, date_time_uptime_to_unix_time_ms, int64_t *);
FAKE_VALUE_FUNC(int, task_wdt_feed, int);
FAKE_VALUE_FUNC(int, task_wdt_add, uint32_t, task_wdt_callback_t, void *);

LOG_MODULE_REGISTER(environmental_module_test, 4);

static const struct device *const sensor_dev = DEVICE_DT_GET(DT_ALIAS(gas_sensor));
static struct payload received_payload;

static int date_time_uptime_to_unix_time_ms_custom_fake(int64_t *time)
{
	*time = 1716552398505;
	return 0;
}

void transport_callback(const struct zbus_channel *chan)
{
	if (chan == &PAYLOAD_CHAN) {
		const struct payload *payload = zbus_chan_const_msg(chan);
		memcpy(&received_payload, payload, sizeof(struct payload));
	}
}

void compare_payloads(const struct payload *expected, const struct payload *actual)
{
	if (actual->string_len != expected->string_len || memcmp(expected->string, actual->string, expected->string_len) != 0) {
		LOG_ERR("Payload mismatch");
		char buf[1024];
		size_t i = snprintf(buf, sizeof(buf), ".string_len = %zu, .string = { ", expected->string_len);
		for (size_t j = 0; j < expected->string_len; j++) {
			i += snprintf(buf + i, sizeof(buf) - i, "0x%02x, ", expected->string[j] & 0xff);
		}
		snprintf(buf + i, sizeof(buf) - i, " }");
		LOG_ERR("C definition of expected payload: %s", buf);
		TEST_FAIL();
	}
}


ZBUS_LISTENER_DEFINE(transport, transport_callback);
ZBUS_LISTENER_DEFINE(trigger, NULL);

/* define unused subscribers */
ZBUS_SUBSCRIBER_DEFINE(location, 1);
ZBUS_SUBSCRIBER_DEFINE(app, 1);
ZBUS_SUBSCRIBER_DEFINE(fota, 1);
ZBUS_SUBSCRIBER_DEFINE(led, 1);
ZBUS_SUBSCRIBER_DEFINE(battery, 2);

/* When changing the format, print the new payload to the console and update the test cases.
 * Don't forget to paste the bytes into a decoder and check if the values are correct.
 */

const struct payload only_timestamp = {
	.string_len = 71,
	.string = {
		0x9f, 0xbf, 0x21, 0x68, 0x31, 0x34, 0x32, 0x30, 0x35, 0x2f, 0x30, 0x2f, 0x00,
		0x61, 0x30, 0x02, 0xfb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22,
		0x1a, 0x66, 0x50, 0x82, 0xce, 0xff, 0xbf, 0x00, 0x61, 0x31, 0x02, 0xfb, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xbf, 0x00, 0x61, 0x32, 0x02,
		0xfb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xbf, 0x00, 0x62,
		0x31, 0x30, 0x02, 0x00, 0xff, 0xff,
	}
};

const struct payload common_case = {
	.string_len = 72,
	.string = {
		0x9f, 0xbf, 0x21, 0x68, 0x31, 0x34, 0x32, 0x30, 0x35, 0x2f, 0x30, 0x2f, 0x00,
		0x61, 0x30, 0x02, 0xfb, 0x40, 0x39, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22,
		0x1a, 0x66, 0x50, 0x82, 0xce, 0xff, 0xbf, 0x00, 0x61, 0x31, 0x02, 0xfb, 0x40,
		0x49, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xbf, 0x00, 0x61, 0x32, 0x02,
		0xfb, 0x40, 0x8f, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xbf, 0x00, 0x62,
		0x31, 0x30, 0x02, 0x18, 0x64, 0xff, 0xff,
	}
};

void setUp(void)
{
	const struct zbus_channel *chan;
	struct gas_sensor_dummy_data *data = sensor_dev->data;

	/* reset fakes */
	RESET_FAKE(task_wdt_feed);
	RESET_FAKE(task_wdt_add);
	RESET_FAKE(date_time_uptime_to_unix_time_ms);

	/* reset static stuff */
	received_payload = (struct payload){0};
	memset(data, 0, sizeof(struct gas_sensor_dummy_data));

	/* clear all channels */
	zbus_sub_wait(&location, &chan, K_NO_WAIT);
	zbus_sub_wait(&app, &chan, K_NO_WAIT);
	zbus_sub_wait(&fota, &chan, K_NO_WAIT);
	zbus_sub_wait(&led, &chan, K_NO_WAIT);
	zbus_sub_wait(&battery, &chan, K_NO_WAIT);
}

void test_only_timestamp(void)
{
	enum trigger_type trigger_type = TRIGGER_DATA_SAMPLE;
	int err;
	enum time_status time_status = TIME_AVAILABLE;

	date_time_uptime_to_unix_time_ms_fake.custom_fake =
		date_time_uptime_to_unix_time_ms_custom_fake;

	err = zbus_chan_pub(&TIME_CHAN, &time_status, K_SECONDS(1));
	TEST_ASSERT_EQUAL(0, err);
	k_sleep(K_MSEC(100));

	/* send trigger */
	err = zbus_chan_pub(&TRIGGER_CHAN, &trigger_type, K_SECONDS(1));
	TEST_ASSERT_EQUAL(0, err);

	/* Allow the test thread to sleep so that the DUT's thread is allowed to run. */
	k_sleep(K_MSEC(100));

	/* check payload */
	compare_payloads(&only_timestamp, &received_payload);
}

void test_common_case(void)
{
	struct gas_sensor_dummy_data *data = sensor_dev->data;
	enum trigger_type trigger_type = TRIGGER_DATA_SAMPLE;
	int err;

	data->temperature = 25.5;
	data->pressure = 100000.0;
	data->humidity = 50.0;
	data->iaq = 100;
	data->co2 = 400;
	data->voc = 100;
	date_time_uptime_to_unix_time_ms_fake.custom_fake =
		date_time_uptime_to_unix_time_ms_custom_fake;

	/* send trigger */
	err = zbus_chan_pub(&TRIGGER_CHAN, &trigger_type, K_SECONDS(1));
	TEST_ASSERT_EQUAL(0, err);

	/* Allow the test thread to sleep so that the DUT's thread is allowed to run. */
	k_sleep(K_MSEC(100));

	/* check payload */
	compare_payloads(&common_case, &received_payload);
}

void test_no_events_on_zbus_until_watchdog_timeout(void)
{
	/* Wait without feeding any events to zbus until watch dog timeout. */
	k_sleep(K_SECONDS(CONFIG_APP_ENVIRONMENTAL_WATCHDOG_TIMEOUT_SECONDS));

	/* Check if the watchdog was fed atleast once.*/
	TEST_ASSERT_GREATER_OR_EQUAL(1, task_wdt_feed_fake.call_count);
}

/* This is required to be added to each test. That is because unity's
 * main may return nonzero, while zephyr's main currently must
 * return 0 in all cases (other values are reserved).
 */
extern int unity_main(void);

int main(void)
{
	/* use the runner from test_runner_generate() */
	(void)unity_main();

	return 0;
}
