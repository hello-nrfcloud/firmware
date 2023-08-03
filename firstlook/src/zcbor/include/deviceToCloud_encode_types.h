/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Generated using zcbor version 0.7.0
 * https://github.com/NordicSemiconductor/zcbor
 * Generated with a --default-max-qty of 10
 */

#ifndef DEVICETOCLOUD_ENCODE_TYPES_H__
#define DEVICETOCLOUD_ENCODE_TYPES_H__

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

struct gnss_message_speed {
	float value;
};

struct gnss_message_heading {
	float value;
};

struct gnss_message_altitude {
	float value;
};

struct gnss_message {
	uint32_t timestamp;
	float latitude;
	float longitude;
	float accuracy;
	struct gnss_message_speed speed;
	bool speed_present;
	struct gnss_message_heading heading;
	bool heading_present;
	struct gnss_message_altitude altitude;
	bool altitude_present;
};

struct button_message {
	uint32_t timestamp;
	uint32_t button_id;
};

struct temperature_message {
	uint32_t timestamp;
	float temperature;
};

struct humidity_message {
	uint32_t timestamp;
	uint32_t humidity;
};

struct air_pressure_message {
	uint32_t timestamp;
	uint32_t air_pressure;
};

struct air_quality_message {
	uint32_t timestamp;
	uint32_t air_quality;
};

struct rsrp_message {
	uint32_t timestamp;
	int32_t rsrp;
};

struct battery_message {
	uint32_t timestamp;
	uint32_t battery_percentage;
};

struct solar_gain_message {
	uint32_t timestamp;
	float solar_gain;
};

struct movement_event_message {
	uint32_t timestamp;
	enum {
		_movement_event_message_movement_event__movement_event_shake = 0,
		_movement_event_message_movement_event__movement_event_tap = 1,
		_movement_event_message_movement_event__movement_event_doubletap = 2,
		_movement_event_message_movement_event__movement_event_turn90 = 3,
		_movement_event_message_movement_event__movement_event_turn180 = 4,
		_movement_event_message_movement_event__movement_event_falling = 5,
		_movement_event_message_movement_event__movement_event_impact = 6,
	} movement_event_choice;
};

struct movement_state_message {
	uint32_t timestamp;
	enum {
		_movement_state_message_movement_state__movement_state_normal = 0,
		_movement_state_message_movement_state__movement_state_tilted = 1,
		_movement_state_message_movement_state__movement_state_upsidedown = 2,
	} movement_state_choice;
};

struct deviceToCloud_message_union_ {
	union {
		struct gnss_message _gnss_message;
		struct button_message _button_message;
		struct temperature_message _temperature_message;
		struct humidity_message _humidity_message;
		struct air_pressure_message _air_pressure_message;
		struct air_quality_message _air_quality_message;
		struct rsrp_message _rsrp_message;
		struct battery_message _battery_message;
		struct solar_gain_message _solar_gain_message;
		struct movement_event_message _movement_event_message;
		struct movement_state_message _movement_state_message;
	};
	enum {
		_deviceToCloud_message_union__gnss_message,
		_deviceToCloud_message_union__button_message,
		_deviceToCloud_message_union__temperature_message,
		_deviceToCloud_message_union__humidity_message,
		_deviceToCloud_message_union__air_pressure_message,
		_deviceToCloud_message_union__air_quality_message,
		_deviceToCloud_message_union__rsrp_message,
		_deviceToCloud_message_union__battery_message,
		_deviceToCloud_message_union__solar_gain_message,
		_deviceToCloud_message_union__movement_event_message,
		_deviceToCloud_message_union__movement_state_message,
	} union_choice;
};

struct deviceToCloud_message {
	struct deviceToCloud_message_union_ _union[10];
	size_t union_count;
};

#ifdef __cplusplus
}
#endif

#endif /* DEVICETOCLOUD_ENCODE_TYPES_H__ */
