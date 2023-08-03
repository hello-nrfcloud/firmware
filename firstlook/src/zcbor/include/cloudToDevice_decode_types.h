/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Generated using zcbor version 0.7.0
 * https://github.com/NordicSemiconductor/zcbor
 * Generated with a --default-max-qty of 10
 */

#ifndef CLOUDTODEVICE_DECODE_TYPES_H__
#define CLOUDTODEVICE_DECODE_TYPES_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif

/** Which value for --default-max-qty this file was created with.
 *
 *  The define is used in the other generated file to do a build-time
 *  compatibility check.
 *
 *  See `zcbor --help` for more information about --default-max-qty
 */
#define DEFAULT_MAX_QTY 10

struct led_message {
	uint32_t timestamp;
	uint32_t led_red;
	uint32_t led_green;
	uint32_t led_blue;
};

struct config_message {
	uint32_t timestamp;
	bool gnss_enable;
	bool gnss_enable_present;
	uint32_t sampling_period;
	bool sampling_period_present;
};

struct cloudToDevice_message_union_ {
	union {
		struct led_message _led_message;
		struct config_message _config_message;
	};
	enum {
		_cloudToDevice_message_union__led_message,
		_cloudToDevice_message_union__config_message,
	} union_choice;
};

struct cloudToDevice_message {
	struct cloudToDevice_message_union_ _union[10];
	size_t union_count;
};

#ifdef __cplusplus
}
#endif

#endif /* CLOUDTODEVICE_DECODE_TYPES_H__ */
