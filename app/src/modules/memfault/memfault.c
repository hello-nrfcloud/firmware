/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include "memfault/components.h"
#include <memfault/ports/zephyr/http.h>
#include <memfault/metrics/metrics.h>
#include <memfault/core/data_packetizer.h>
#include <memfault/core/trace_event.h>
#include "memfault/panics/coredump.h"
#include <modem/nrf_modem_lib_trace.h>

#include "message_channel.h"

LOG_MODULE_REGISTER(memfault, CONFIG_APP_MEMFAULT_LOG_LEVEL);

#if defined(CONFIG_NRF_MODEM_LIB_TRACE)

static const char *mimetypes[] = { MEMFAULT_CDR_BINARY };

static bool has_modem_traces;

static sMemfaultCdrMetadata trace_recording_metadata = {
	.start_time.type = kMemfaultCurrentTimeType_Unknown,
	.mimetypes = mimetypes,
	.num_mimetypes = 1,
	.data_size_bytes = 0,
	.collection_reason = "modem traces",
};

static bool enable_modem_traces(void)
{
	int err = nrf_modem_lib_trace_level_set(NRF_MODEM_LIB_TRACE_LEVEL_FULL);

	if (err) {
		LOG_ERR("Failed to enable modem traces: %d", err);
		return false;
	}

	return true;
}

static bool has_cdr_cb(sMemfaultCdrMetadata *metadata)
{
	if (!has_modem_traces) {
		return false;
	}
	*metadata = trace_recording_metadata;
	return true;
}

static void mark_cdr_read_cb(void)
{
	has_modem_traces = false;
}

static bool read_data_cb(uint32_t offset, void *buf, size_t buf_len)
{
	ARG_UNUSED(offset);
	int err = nrf_modem_lib_trace_read(buf, buf_len);

	if (err < 0) {
		LOG_ERR("Failed to read modem trace data: %d", err);
		return false;
	}

	return true;
}

static sMemfaultCdrSourceImpl s_my_custom_data_recording_source = {
	.has_cdr_cb = has_cdr_cb,
	.read_data_cb = read_data_cb,
	.mark_cdr_read_cb = mark_cdr_read_cb,
};

static void prepare_modem_trace_upload(void)
{
	int err;
	size_t trace_size = 0;
	static bool memfault_cdr_source_registered;

	trace_size = nrf_modem_lib_trace_data_size();

	if (trace_size == -ENOTSUP) {
		LOG_ERR("The current modem trace backend is not supported");
		return;
	} else if (trace_size < 0) {
		LOG_ERR("Failed to get modem trace size: %d", trace_size);
		return;
	} else if (trace_size == 0) {
		LOG_DBG("No modem traces to send");
		return;
	}

	/* Stop modem traces during upload */
	err = nrf_modem_lib_trace_level_set(NRF_MODEM_LIB_TRACE_LEVEL_OFF);
	if (err) {
		LOG_ERR("Failed to turn off modem traces: %d", err);
		return;
	}

	LOG_DBG("Preparing modem trace data upload of: %d bytes", trace_size);

	if (!memfault_cdr_source_registered) {
		memfault_cdr_register_source(&s_my_custom_data_recording_source);
		memfault_cdr_source_registered = true;
	}

	trace_recording_metadata.duration_ms = 0;
	trace_recording_metadata.data_size_bytes = trace_size;
	has_modem_traces = true;
}

#endif /* defined(CONFIG_NRF_MODEM_LIB_TRACE) */

static void on_connected(void)
{
	/* Trigger collection of heartbeat data */
	memfault_metrics_heartbeat_debug_trigger();

	/* Check if there is any data available to be sent */
	if (!memfault_packetizer_data_available()) {
		return;
	}

#if defined(CONFIG_NRF_MODEM_LIB_TRACE)
	size_t total_size = 0;

	/* If there was a coredump, also send modem trace */
	if (memfault_coredump_has_valid_coredump(&total_size)) {
		prepare_modem_trace_upload();
	}

#endif /* defined(CONFIG_NRF_MODEM_LIB_TRACE) */

	memfault_zephyr_port_post_data();

#if defined(CONFIG_NRF_MODEM_LIB_TRACE)
	enable_modem_traces();
#endif /* defined(CONFIG_NRF_MODEM_LIB_TRACE) */

}

void callback(const struct zbus_channel *chan)
{
	if (&CLOUD_CHAN == chan) {
		const enum cloud_status *status = zbus_chan_const_msg(chan);

		if (*status == CLOUD_CONNECTED_READY_TO_SEND) {
			on_connected();
		}
	}
}

ZBUS_LISTENER_DEFINE(memfault, callback);
ZBUS_CHAN_ADD_OBS(CLOUD_CHAN, memfault, 0);
