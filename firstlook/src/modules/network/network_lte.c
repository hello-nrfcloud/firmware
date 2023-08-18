/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#include "date_time.h"

#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(network, CONFIG_MQTT_SAMPLE_NETWORK_LOG_LEVEL);

/* This module does not subscribe to any channels */

#define L4_EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)
#define CONNECTIVITY_EVENT_MASK (NET_EVENT_CONN_IF_FATAL_ERROR)

static struct net_mgmt_event_callback l4_cb;
static struct net_mgmt_event_callback conn_cb;
static bool network_connected_pending;

static void l4_event_handler(struct net_mgmt_event_callback *cb,
			     uint32_t event,
			     struct net_if *iface)
{
	int err;
	enum network_status status;
	int64_t timestamp;

	switch (event) {
	case NET_EVENT_L4_CONNECTED:
		LOG_INF("IP Up");
		status = NETWORK_CONNECTED;
		err = date_time_now(&timestamp);
		if (err) {
			LOG_INF("NETWORK_CONNECTED is sent later when time is aquired.");
			network_connected_pending = true;
			return;
		}
	case NET_EVENT_L4_DISCONNECTED:
		LOG_INF("IP down");
		status = NETWORK_DISCONNECTED;
		break;
	default:
		/* Don't care */
		return;
	}

	err = zbus_chan_pub(&NETWORK_CHAN, &status, K_SECONDS(1));
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
	}
}

static void connectivity_event_handler(struct net_mgmt_event_callback *cb,
				       uint32_t event,
				       struct net_if *iface)
{
	if (event == NET_EVENT_CONN_IF_FATAL_ERROR) {
		LOG_ERR("Fatal error received from the connectivity layer");
		SEND_FATAL_ERROR();
		return;
	}
}

static void date_time_handler(const struct date_time_evt *evt) {
	int err;
	enum network_status status = NETWORK_CONNECTED;

	if (evt->type != DATE_TIME_NOT_OBTAINED && network_connected_pending) {
		network_connected_pending = false;
		LOG_INF("time aquired, sending NETWORK_CONNECTED");
		err = zbus_chan_pub(&NETWORK_CHAN, &status, K_SECONDS(1));
		if (err) {
			LOG_ERR("zbus_chan_pub, error: %d", err);
			SEND_FATAL_ERROR();
		}
	}
}


static void network_task(void)
{
	int err;
	const struct device *lte_dev = device_get_binding("nrf91_socket");
	struct net_if *net_if = net_if_lookup_by_dev(lte_dev);
	net_if_set_default(net_if);

	net_mgmt_init_event_callback(&l4_cb, l4_event_handler, L4_EVENT_MASK);
	net_mgmt_add_event_callback(&l4_cb);

	net_mgmt_init_event_callback(&conn_cb, connectivity_event_handler, CONNECTIVITY_EVENT_MASK);
	net_mgmt_add_event_callback(&conn_cb);

	date_time_register_handler(date_time_handler);

	LOG_INF("Bringing network interface up");

	err = net_if_up(net_if);
	if (err) {
		LOG_ERR("net_if_up, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	LOG_INF("Connecting...");

	err = conn_mgr_if_connect(net_if);
	if (err) {
		LOG_ERR("conn_mgr_if_connect, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}
}

K_THREAD_DEFINE(network_task_id,
		CONFIG_MQTT_SAMPLE_NETWORK_THREAD_STACK_SIZE,
		network_task, NULL, NULL, NULL, 3, 0, 0);
