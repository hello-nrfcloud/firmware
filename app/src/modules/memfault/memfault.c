/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <memfault/ports/zephyr/http.h>
#include <memfault/metrics/metrics.h>
#include <memfault/core/data_packetizer.h>
#include <memfault/core/trace_event.h>

#include "message_channel.h"

static void on_connected(void)
{
	/* Trigger collection of heartbeat data */
	memfault_metrics_heartbeat_debug_trigger();

	/* Check if there is any data available to be sent */
	if (!memfault_packetizer_data_available()) {
		return;
	}

	memfault_zephyr_port_post_data();
}

void callback(const struct zbus_channel *chan)
{
	if (&CLOUD_CHAN == chan) {
		const enum cloud_status *status = zbus_chan_const_msg(chan);

		if (*status == CLOUD_CONNECTED_READY_TO_SEND) {
			on_connected();
		}
	}

	if (&ERROR_CHAN == chan) {
		const enum error_type *type = zbus_chan_const_msg(chan);

		if (*type == ERROR_DECODE) {
			MEMFAULT_TRACE_EVENT(decode_error);
		}
	}
}

ZBUS_LISTENER_DEFINE(memfault, callback);
ZBUS_CHAN_ADD_OBS(CLOUD_CHAN, memfault, 0);
ZBUS_CHAN_ADD_OBS(ERROR_CHAN, memfault, 0);
