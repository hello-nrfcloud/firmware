/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/task_wdt/task_wdt.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/init.h>

#include <modem/location.h>
#include <nrf_modem_gnss.h>
#include <date_time.h>

#include "message_channel.h"
#include "modem/lte_lc.h"

#include <net/nrf_cloud.h>

LOG_MODULE_REGISTER(location_module, CONFIG_APP_LOCATION_LOG_LEVEL);

BUILD_ASSERT(CONFIG_APP_LOCATION_WATCHDOG_TIMEOUT_SECONDS >
	     CONFIG_APP_LOCATION_ZBUS_TIMEOUT_SECONDS,
	     "Watchdog timeout must be greater than trigger timeout");

/* Define listener for this module */
ZBUS_SUBSCRIBER_DEFINE(location, CONFIG_APP_LOCATION_ZBUS_QUEUE_SIZE);

/* Observe channels */
ZBUS_CHAN_ADD_OBS(TRIGGER_CHAN, location, 0);
ZBUS_CHAN_ADD_OBS(CLOUD_CHAN, location, 0);
ZBUS_CHAN_ADD_OBS(CONFIG_CHAN, location, 0);
ZBUS_CHAN_ADD_OBS(NETWORK_CHAN, location, 0);

static bool gnss_enabled;
static bool gnss_initialized;

static void location_event_handler(const struct location_event_data *event_data);

int nrf_cloud_coap_location_send(const struct nrf_cloud_gnss_data *gnss, bool confirmable);

static void task_wdt_callback(int channel_id, void *user_data)
{
	LOG_ERR("Watchdog expired, Channel: %d, Thread: %s",
		channel_id, k_thread_name_get((k_tid_t)user_data));

	SEND_FATAL_ERROR_WATCHDOG_TIMEOUT();
}

static const int gnss_method_index = 0;
static enum location_method location_method_types[] = {
	LOCATION_METHOD_GNSS,
	LOCATION_METHOD_WIFI,
	LOCATION_METHOD_CELLULAR
};
static const uint8_t location_methods_size = ARRAY_SIZE(location_method_types);

void trigger_location_update(void)
{
	int err;
	struct location_config config = {0};

	if (gnss_enabled) {
		location_config_defaults_set(&config, location_methods_size, location_method_types);
		config.methods[gnss_method_index].gnss.visibility_detection=true;
		LOG_DBG("GNSS enabled");
	} else {
		/* Only pass in a subset of the location methods to skip GNSS */
		location_config_defaults_set(&config, location_methods_size - 1, location_method_types + 1);
		LOG_DBG("GNSS disabled");
	}

	LOG_DBG("location library initialized");

	err = location_request(&config);
	if (err == -EBUSY) {
		LOG_WRN("Location request already in progress");
	} else if (err) {
		LOG_ERR("Unable to send location request: %d", err);
	}
}

void handle_network_chan(void) {
	int err = 0;
	enum network_status status;

	if (gnss_initialized) {
		return;
	}

	err = zbus_chan_read(&NETWORK_CHAN, &status, K_SECONDS(1));
	if (err) {
		LOG_ERR("Failed to read network status: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	if (status == NETWORK_CONNECTED) {
		/* GNSS has to be enabled after the modem is initialized and enabled */
		err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_ACTIVATE_GNSS);
		if (err) {
			LOG_ERR("Unable to init GNSS: %d", err);
			SEND_FATAL_ERROR();
		} else {
			gnss_initialized = true;
			LOG_DBG("GNSS initialized");
		}
	}
}

void handle_trigger_chan(void)
{
	int err = 0;
	enum trigger_type trigger_type = 0;

	err = zbus_chan_read(&TRIGGER_CHAN, &trigger_type, K_SECONDS(1));
	if (err) {
		LOG_ERR("Failed to read trigger from zbus: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	if (trigger_type == TRIGGER_DATA_SAMPLE) {
		LOG_DBG("Data sample trigger received, getting location");
		trigger_location_update();
	}
}

void handle_config_chan(void)
{
	int err = 0;
	struct configuration config = { 0 };

	err = zbus_chan_read(&CONFIG_CHAN, &config, K_SECONDS(1));
	if (err) {
		LOG_ERR("Failed to read configuration from zbus: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	if (config.config_present) {
		gnss_enabled = config.gnss;
		LOG_DBG("GNSS enabled: %d", gnss_enabled);
	} else {
		LOG_DBG("Configuration not present");
	}
}

void location_task(void)
{
	int err = 0;
	const struct zbus_channel *chan;
	int task_wdt_id;
	const uint32_t wdt_timeout_ms = (CONFIG_APP_LOCATION_WATCHDOG_TIMEOUT_SECONDS * MSEC_PER_SEC);
	const k_timeout_t zbus_timeout = K_SECONDS(CONFIG_APP_LOCATION_ZBUS_TIMEOUT_SECONDS);

	LOG_DBG("Location module task started");

	task_wdt_id = task_wdt_add(wdt_timeout_ms, task_wdt_callback, (void *)k_current_get());
	if (task_wdt_id < 0) {
		LOG_ERR("Failed to add task to watchdog: %d", task_wdt_id);
		SEND_FATAL_ERROR();
		return;
	}

	err = location_init(location_event_handler);
	if (err) {
		LOG_ERR("Unable to init location library: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	LOG_DBG("location library initialized");

	while (true) {
		err = task_wdt_feed(task_wdt_id);
		if (err) {
			LOG_ERR("Failed to feed the watchdog: %d", err);
			SEND_FATAL_ERROR();
			return;
		}

		err = zbus_sub_wait(&location, &chan, zbus_timeout);
		if (err == -EAGAIN) {
			continue;
		} else if (err) {
			LOG_ERR("zbus_sub_wait, error: %d", err);
			SEND_FATAL_ERROR();
			return;
		}

		if (&NETWORK_CHAN == chan) {
			LOG_DBG("Network status received");
			handle_network_chan();
		}

		if (&TRIGGER_CHAN == chan) {
			LOG_DBG("Trigger received");
			handle_trigger_chan();
		}

		if (&CONFIG_CHAN == chan) {
			LOG_DBG("Configuration received");
			handle_config_chan();
		}
	}
}

K_THREAD_DEFINE(location_module_tid, CONFIG_APP_LOCATION_THREAD_STACK_SIZE,
		location_task, NULL, NULL, NULL,
		K_HIGHEST_APPLICATION_THREAD_PRIO, 0, 0);

/* Take time from PVT data and apply it to system time. */
static void apply_gnss_time(const struct nrf_modem_gnss_pvt_data_frame *pvt_data)
{
	struct tm gnss_time = {
		.tm_year = pvt_data->datetime.year - 1900,
		.tm_mon = pvt_data->datetime.month - 1,
		.tm_mday = pvt_data->datetime.day,
		.tm_hour = pvt_data->datetime.hour,
		.tm_min = pvt_data->datetime.minute,
		.tm_sec = pvt_data->datetime.seconds,
	};

	date_time_set(&gnss_time);
}

static void location_event_handler(const struct location_event_data *event_data)
{
	switch (event_data->id) {
	case LOCATION_EVT_LOCATION:
		LOG_DBG("Got location: lat: %f, lon: %f, acc: %f",
			(double) event_data->location.latitude,
			(double) event_data->location.longitude,
			(double) event_data->location.accuracy);

		if (event_data->method == LOCATION_METHOD_GNSS) {
			struct nrf_modem_gnss_pvt_data_frame pvt_data =
				event_data->location.details.gnss.pvt_data;
			if (event_data->location.datetime.valid) {
				/* GNSS is the most accurate time source -  use it. */
				apply_gnss_time(&pvt_data);
			} else {
				/* this should not happen */
				LOG_WRN("Got GNSS location without valid time data");
			}
		}
		break;
	case LOCATION_EVT_RESULT_UNKNOWN:
		LOG_DBG("Getting location completed with undefined result");
		break;
	case LOCATION_EVT_TIMEOUT:
		LOG_DBG("Getting location timed out");
		break;
	case LOCATION_EVT_ERROR:
		LOG_WRN("Getting location failed");
		break;
	default:
		LOG_DBG("Getting location: Unknown event %d", event_data->id);
		break;
	}
}
