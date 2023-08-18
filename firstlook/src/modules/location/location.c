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
#include "nrf_cloud_coap_transport.h"
#include "net/nrf_cloud_coap.h"

LOG_MODULE_REGISTER(location_module, CONFIG_APP_MODULE_LOCATION_LOG_LEVEL);

static K_SEM_DEFINE(location_lib_sem, 1, 1);
static K_SEM_DEFINE(modem_init_sem, 0, 1);
static K_SEM_DEFINE(trigger_sem, 0, 1);
#define STACKSIZE 4096

static void location_event_handler(const struct location_event_data *event_data);

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

K_THREAD_DEFINE(location_module_tid, STACKSIZE,
                location_module_entry, NULL, NULL, NULL,
                K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

static void trigger_callback(const struct zbus_channel *chan)
{
    if (&TRIGGER_CHAN == chan)
    {
        k_sem_give(&trigger_sem);
    }
    if (&NETWORK_CHAN == chan)
    {
        enum network_status status = NETWORK_DISCONNECTED;
        int err = zbus_chan_read(chan, &status, K_NO_WAIT);

        if (!err && status == NETWORK_CONNECTED)
        {
            k_sem_give(&modem_init_sem);
        }
    }
}

ZBUS_LISTENER_DEFINE(location, trigger_callback);

static void location_event_handler(const struct location_event_data *event_data)
{
    switch (event_data->id)
    {
    case LOCATION_EVT_LOCATION:
        LOG_DBG("Got location: lat: %f, lon: %f, acc: %f",
                event_data->location.latitude,
                event_data->location.longitude,
                event_data->location.accuracy);
        // TODO send gnss location
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
