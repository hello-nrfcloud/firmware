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

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Macro used to send a message on the FATAL_ERROR_CHANNEL.
 *	   The message will be handled in the led module.
 */
#define SEND_FATAL_ERROR() do {							\
	int not_used = -1;							\
	(void)zbus_chan_pub(&FATAL_ERROR_CHAN, &not_used, K_SECONDS(10));	\
	LOG_PANIC();								\
	k_sleep(K_SECONDS(5));							\
	__ASSERT(false, "SEND_FATAL_ERROR() macro called");			\
} while (0)

struct payload {
	char string[CONFIG_APP_PAYLOAD_CHANNEL_STRING_MAX_SIZE];
	size_t string_len;
};

enum network_status {
	NETWORK_DISCONNECTED = 0x1,
	NETWORK_CONNECTED,
};

enum cloud_status {
	CLOUD_CONNECTED = 0x1,
	CLOUD_DISCONNECTED,
};

struct configuration {
	int led_red;
	int led_green;
	int led_blue;
	bool gnss;
	uint64_t update_interval;
};

ZBUS_CHAN_DECLARE(
	TRIGGER_CHAN,
	PAYLOAD_CHAN,
	NETWORK_CHAN,
	FATAL_ERROR_CHAN,
	LED_CHAN,
	CLOUD_CHAN,
	FOTA_ONGOING_CHAN,
	CONFIG_CHAN
);

#ifdef __cplusplus
}
#endif

#endif /* _MESSAGE_CHANNEL_H_ */
