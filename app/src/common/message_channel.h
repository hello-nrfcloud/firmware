/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _MESSAGE_CHANNEL_H_
#define _MESSAGE_CHANNEL_H_

#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/zbus/zbus.h>
#if defined(CONFIG_MEMFAULT)
#include <memfault/panics/assert.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Handle fatal error.
 *  @param is_watchdog_timeout Boolean indicating if the macro was called upon a watchdog timeout.
 */
#define FATAL_ERROR_HANDLE(is_watchdog_timeout) do {				\
	enum error_type type = ERROR_FATAL;					\
	(void)zbus_chan_pub(&ERROR_CHAN, &type, K_SECONDS(10));			\
	LOG_PANIC();								\
	if (is_watchdog_timeout) {						\
		IF_ENABLED(CONFIG_MEMFAULT, (MEMFAULT_SOFTWARE_WATCHDOG()));	\
	}									\
	k_sleep(K_SECONDS(5));							\
	__ASSERT(false, "SEND_FATAL_ERROR() macro called");			\
} while (0)

/** @brief Macro used to handle fatal errors. */
#define SEND_FATAL_ERROR() FATAL_ERROR_HANDLE(0)
/** @brief Macro used to handle watchdog timeouts. */
#define SEND_FATAL_ERROR_WATCHDOG_TIMEOUT() FATAL_ERROR_HANDLE(1)

struct payload {
	char string[CONFIG_APP_PAYLOAD_CHANNEL_STRING_MAX_SIZE];
	size_t string_len;
};

enum network_status {
	NETWORK_DISCONNECTED = 0x1,
	NETWORK_CONNECTED,
};

enum cloud_status {
	CLOUD_DISCONNECTED = 0x1,
	CLOUD_CONNECTED_READY_TO_SEND,
	CLOUD_CONNECTED_PAUSED,
};

enum trigger_type {
	TRIGGER_POLL = 0x1,
	TRIGGER_DATA_SAMPLE,
};

enum trigger_mode {
	TRIGGER_MODE_POLL = 0x1,
	TRIGGER_MODE_NORMAL,
};

enum time_status {
	TIME_AVAILABLE = 0x1,
};

enum error_type {
	ERROR_FATAL = 0x1,
	ERROR_DECODE,
};

struct configuration {
	bool led_present;
	int led_red;
	int led_green;
	int led_blue;
	bool config_present;
	bool gnss;
	uint64_t update_interval;
};

ZBUS_CHAN_DECLARE(
	BUTTON_CHAN,
	CLOUD_CHAN,
	CONFIG_CHAN,
	ERROR_CHAN,
	FOTA_ONGOING_CHAN,
	LED_CHAN,
	NETWORK_CHAN,
	PAYLOAD_CHAN,
	TIME_CHAN,
	TRIGGER_CHAN,
	TRIGGER_MODE_CHAN
);

#ifdef __cplusplus
}
#endif

#endif /* _MESSAGE_CHANNEL_H_ */
