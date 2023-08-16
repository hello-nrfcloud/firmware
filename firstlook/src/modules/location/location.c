/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/init.h>

#include <modem/location.h>

#include "message_channel.h"
#include "modem/lte_lc.h"

LOG_MODULE_REGISTER(location_module, CONFIG_APP_MODULE_LOCATION_LOG_LEVEL);

static K_SEM_DEFINE(location_lib_sem, 1, 1);
static K_SEM_DEFINE(modem_init_sem, 0, 1);
static K_SEM_DEFINE(trigger_sem, 0, 1);
#define STACKSIZE               4096

static void location_event_handler(const struct location_event_data *event_data);

void location_module_entry(void) {
    int err = 0;
    struct location_config config = { 0 };
    enum location_method chosen_methods[] = {
#if defined(CONFIG_LOCATION_METHOD_CELLULAR)
        LOCATION_METHOD_CELLULAR,
#endif
#if defined(CONFIG_LOCATION_METHOD_WIFI)
        LOCATION_METHOD_WIFI,
#endif
#if defined(CONFIG_LOCATION_METHOD_GNSS)
        LOCATION_METHOD_GNSS,
#endif
    };
    k_sem_take(&modem_init_sem, K_FOREVER);

    err = location_init(location_event_handler);
    if (err) {
        LOG_ERR("Unable to init location library: %d", err);
    }
    LOG_DBG("location library initialized");

#if defined(CONFIG_LOCATION_METHOD_GNSS)
    err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_ACTIVATE_GNSS);
    if (err) {
        LOG_ERR("Unable to init GNSS: %d", err);
    }
#endif

    while (true) {
        k_sem_take(&trigger_sem, K_FOREVER);
        for (size_t i = 0; i < ARRAY_SIZE(chosen_methods); ++i) {
            location_config_defaults_set(&config, 1, &chosen_methods[i]);
            k_sem_take(&location_lib_sem, K_FOREVER);
            err = location_request(&config);
            if (err) {
                LOG_ERR("Unable to send location request: %d", err);
            }
        }
    }
}

K_THREAD_DEFINE(location_module_tid, STACKSIZE,
                location_module_entry, NULL, NULL, NULL,
                K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

static void trigger_callback(const struct zbus_channel *chan)
{
    if (&TRIGGER_CHAN == chan) {
        k_sem_give(&trigger_sem);
    }
    if (&NETWORK_CHAN == chan) {
        enum network_status status = NETWORK_DISCONNECTED;
        int err = zbus_chan_read(chan, &status, K_NO_WAIT);

        if (!err && status == NETWORK_CONNECTED) {
            k_sem_give(&modem_init_sem);
        }
    }
}

ZBUS_LISTENER_DEFINE(location, trigger_callback);

static void location_event_handler(const struct location_event_data *event_data)
{
	switch (event_data->id) {
	case LOCATION_EVT_LOCATION:
		LOG_DBG("Got location: lat: %f, lon: %f, acc: %f",
            event_data->location.latitude,
            event_data->location.longitude,
            event_data->location.accuracy);
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
	case LOCATION_EVT_GNSS_ASSISTANCE_REQUEST:
		LOG_DBG("Requested A-GPS data");
		break;
	case LOCATION_EVT_GNSS_PREDICTION_REQUEST:
		LOG_DBG("Requested P-GPS data");
		break;
	case LOCATION_EVT_CLOUD_LOCATION_EXT_REQUEST:
		LOG_DBG("Getting cloud location request");
        if (event_data->cloud_location_request.cell_data) {
            LOG_DBG("%d cells", event_data->cloud_location_request.cell_data->ncells_count);
        }
        if (event_data->cloud_location_request.wifi_data) {
            LOG_DBG("%d aps", event_data->cloud_location_request.wifi_data->cnt);
        }
        location_cloud_location_ext_result_set(LOCATION_EXT_RESULT_UNKNOWN, NULL);
		break;
	default:
		LOG_DBG("Getting location: Unknown event %d", event_data->id);
		break;
	}
    if (event_data->id == LOCATION_EVT_LOCATION ||
        event_data->id == LOCATION_EVT_RESULT_UNKNOWN ||
        event_data->id == LOCATION_EVT_TIMEOUT ||
        event_data->id == LOCATION_EVT_ERROR) {
        /* request completed */
        k_sem_give(&location_lib_sem);
    }
}
