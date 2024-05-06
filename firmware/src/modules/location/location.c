/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/init.h>

#include <modem/location.h>
#include <nrf_modem_gnss.h>
#include <date_time.h>

#include "message_channel.h"
#include "modem/lte_lc.h"

#include <net/nrf_cloud.h>

LOG_MODULE_REGISTER(location_module, CONFIG_APP_LOCATION_LOG_LEVEL);

static K_SEM_DEFINE(location_lib_sem, 1, 1);
static K_SEM_DEFINE(modem_init_sem, 0, 1);
static K_SEM_DEFINE(trigger_sem, 0, 1);

static void location_event_handler(const struct location_event_data *event_data);

int nrf_cloud_coap_location_send(const struct nrf_cloud_gnss_data *gnss, bool confirmable);

void location_module_entry(void)
{
	int err = 0;
	struct location_config config = {0};

	k_sem_take(&modem_init_sem, K_FOREVER);

	err = location_init(location_event_handler);
	if (err)
	{
		LOG_ERR("Unable to init location library: %d", err);
	}
	LOG_DBG("location library initialized");

#if defined(CONFIG_LOCATION_METHOD_GNSS)
	err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_ACTIVATE_GNSS);
	if (err)
	{
		LOG_ERR("Unable to init GNSS: %d", err);
	} else {
		LOG_DBG("GNSS initialized");
	}
#endif

	while (true)
	{
		k_sem_take(&trigger_sem, K_FOREVER);
		location_config_defaults_set(&config, 0, NULL);
		config.mode = LOCATION_REQ_MODE_ALL;
		err = location_request(&config);
		if (err)
		{
			LOG_ERR("Unable to send location request: %d", err);
		}
		k_sem_take(&location_lib_sem, K_FOREVER);
	}
}

K_THREAD_DEFINE(location_module_tid, CONFIG_APP_LOCATION_THREAD_STACK_SIZE,
		location_module_entry, NULL, NULL, NULL,
		K_HIGHEST_APPLICATION_THREAD_PRIO, 0, 0);

static void trigger_callback(const struct zbus_channel *chan)
{
	if (&TRIGGER_CHAN == chan)
	{
		LOG_DBG("Trigger received");
		k_sem_give(&trigger_sem);
	}
	if (&CLOUD_CHAN == chan)
	{
		LOG_DBG("Cloud status received");
		enum cloud_status *status = zbus_chan_const_msg(chan);

		if (*status == CLOUD_CONNECTED)
		{
			LOG_DBG("Cloud connected, initializing location");
			k_sem_give(&modem_init_sem);
		}
	}
}

ZBUS_LISTENER_DEFINE(location, trigger_callback);

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

static void report_gnss_location(const struct nrf_modem_gnss_pvt_data_frame *pvt_data)
{
	int err;
	/* speed (velocity) and heading are only available when there are multiple fixes */
	/* we assume an altitude to be available (can only be missing on low-accuracy mode) */
	struct nrf_cloud_gnss_data gnss_pvt = {
		.type = NRF_CLOUD_GNSS_TYPE_PVT,
		.ts_ms = NRF_CLOUD_NO_TIMESTAMP,
		.pvt = {
			.lon =		pvt_data->longitude,
			.lat =		pvt_data->latitude,
			.accuracy =	pvt_data->accuracy,
			.alt =		pvt_data->altitude,
			.has_alt =	1,
			.speed =	pvt_data->speed,
			.has_speed =	pvt_data->flags & NRF_MODEM_GNSS_PVT_FLAG_VELOCITY_VALID,
			.heading =	pvt_data->heading,
			.has_heading =	pvt_data->flags & NRF_MODEM_GNSS_PVT_FLAG_VELOCITY_VALID
		}
	};

	err = nrf_cloud_coap_location_send(&gnss_pvt, true);
	if (err)
	{
		LOG_ERR("Failed to send location data: %d", err);
	}
}

static void location_event_handler(const struct location_event_data *event_data)
{
	switch (event_data->id)
	{
	case LOCATION_EVT_LOCATION:
		LOG_DBG("Got location: lat: %f, lon: %f, acc: %f",
			(double) event_data->location.latitude,
			(double) event_data->location.longitude,
			(double) event_data->location.accuracy);

		/* GNSS location needs to be reported manually */
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
			report_gnss_location(&pvt_data);
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
	if (event_data->id == LOCATION_EVT_LOCATION ||
	    event_data->id == LOCATION_EVT_RESULT_UNKNOWN ||
	    event_data->id == LOCATION_EVT_TIMEOUT ||
	    event_data->id == LOCATION_EVT_ERROR)
	{
		/* request completed */
		k_sem_give(&location_lib_sem);
	}
}
