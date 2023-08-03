/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Generated using zcbor version 0.7.0
 * https://github.com/NordicSemiconductor/zcbor
 * Generated with a --default-max-qty of 10
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "zcbor_encode.h"
#include "deviceToCloud_encode.h"

#if DEFAULT_MAX_QTY != 10
#error "The type file was generated with a different default_max_qty than this file"
#endif

static bool encode_repeated_gnss_message_speed(zcbor_state_t *state, const struct gnss_message_speed *input);
static bool encode_repeated_gnss_message_heading(zcbor_state_t *state, const struct gnss_message_heading *input);
static bool encode_repeated_gnss_message_altitude(zcbor_state_t *state, const struct gnss_message_altitude *input);
static bool encode_gnss_message(zcbor_state_t *state, const struct gnss_message *input);
static bool encode_button_message(zcbor_state_t *state, const struct button_message *input);
static bool encode_temperature_message(zcbor_state_t *state, const struct temperature_message *input);
static bool encode_humidity_message(zcbor_state_t *state, const struct humidity_message *input);
static bool encode_air_pressure_message(zcbor_state_t *state, const struct air_pressure_message *input);
static bool encode_air_quality_message(zcbor_state_t *state, const struct air_quality_message *input);
static bool encode_rsrp_message(zcbor_state_t *state, const struct rsrp_message *input);
static bool encode_battery_message(zcbor_state_t *state, const struct battery_message *input);
static bool encode_solar_gain_message(zcbor_state_t *state, const struct solar_gain_message *input);
static bool encode_movement_event_message(zcbor_state_t *state, const struct movement_event_message *input);
static bool encode_movement_state_message(zcbor_state_t *state, const struct movement_state_message *input);
static bool encode_repeated_deviceToCloud_message_union(zcbor_state_t *state, const struct deviceToCloud_message_union_ *input);
static bool encode_deviceToCloud_message(zcbor_state_t *state, const struct deviceToCloud_message *input);


static bool encode_repeated_gnss_message_speed(
		zcbor_state_t *state, const struct gnss_message_speed *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((((zcbor_uint32_put(state, (1))))
	&& ((zcbor_float32_encode(state, (&(*input).value)))))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_repeated_gnss_message_heading(
		zcbor_state_t *state, const struct gnss_message_heading *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((((zcbor_uint32_put(state, (2))))
	&& ((zcbor_float32_encode(state, (&(*input).value)))))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_repeated_gnss_message_altitude(
		zcbor_state_t *state, const struct gnss_message_altitude *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((((zcbor_uint32_put(state, (3))))
	&& ((zcbor_float32_encode(state, (&(*input).value)))))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_gnss_message(
		zcbor_state_t *state, const struct gnss_message *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_encode(state, 11) && ((((zcbor_uint32_put(state, (1))))
	&& (((((*input).timestamp <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input).timestamp))))
	&& ((zcbor_float32_encode(state, (&(*input).latitude))))
	&& ((zcbor_float32_encode(state, (&(*input).longitude))))
	&& ((zcbor_float32_encode(state, (&(*input).accuracy))))
	&& zcbor_present_encode(&((*input).speed_present), (zcbor_encoder_t *)encode_repeated_gnss_message_speed, state, (&(*input).speed))
	&& zcbor_present_encode(&((*input).heading_present), (zcbor_encoder_t *)encode_repeated_gnss_message_heading, state, (&(*input).heading))
	&& zcbor_present_encode(&((*input).altitude_present), (zcbor_encoder_t *)encode_repeated_gnss_message_altitude, state, (&(*input).altitude))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 11))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_button_message(
		zcbor_state_t *state, const struct button_message *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_encode(state, 3) && ((((zcbor_uint32_put(state, (2))))
	&& (((((*input).timestamp <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input).timestamp))))
	&& (((((*input).button_id <= UINT8_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input).button_id))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 3))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_temperature_message(
		zcbor_state_t *state, const struct temperature_message *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_encode(state, 3) && ((((zcbor_uint32_put(state, (3))))
	&& (((((*input).timestamp <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input).timestamp))))
	&& ((zcbor_float32_encode(state, (&(*input).temperature))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 3))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_humidity_message(
		zcbor_state_t *state, const struct humidity_message *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_encode(state, 3) && ((((zcbor_uint32_put(state, (4))))
	&& (((((*input).timestamp <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input).timestamp))))
	&& (((((*input).humidity >= 0)
	&& ((*input).humidity <= UINT8_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input).humidity))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 3))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_air_pressure_message(
		zcbor_state_t *state, const struct air_pressure_message *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_encode(state, 3) && ((((zcbor_uint32_put(state, (5))))
	&& (((((*input).timestamp <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input).timestamp))))
	&& (((((*input).air_pressure <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input).air_pressure))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 3))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_air_quality_message(
		zcbor_state_t *state, const struct air_quality_message *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_encode(state, 3) && ((((zcbor_uint32_put(state, (6))))
	&& (((((*input).timestamp <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input).timestamp))))
	&& (((((*input).air_quality >= 0)
	&& ((*input).air_quality <= UINT16_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input).air_quality))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 3))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_rsrp_message(
		zcbor_state_t *state, const struct rsrp_message *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_encode(state, 3) && ((((zcbor_uint32_put(state, (7))))
	&& (((((*input).timestamp <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input).timestamp))))
	&& (((((*input).rsrp >= -32767)
	&& ((*input).rsrp <= INT16_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_int32_encode(state, (&(*input).rsrp))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 3))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_battery_message(
		zcbor_state_t *state, const struct battery_message *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_encode(state, 3) && ((((zcbor_uint32_put(state, (8))))
	&& (((((*input).timestamp <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input).timestamp))))
	&& (((((*input).battery_percentage >= 0)
	&& ((*input).battery_percentage <= UINT8_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input).battery_percentage))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 3))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_solar_gain_message(
		zcbor_state_t *state, const struct solar_gain_message *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_encode(state, 3) && ((((zcbor_uint32_put(state, (9))))
	&& (((((*input).timestamp <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input).timestamp))))
	&& ((zcbor_float32_encode(state, (&(*input).solar_gain))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 3))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_movement_event_message(
		zcbor_state_t *state, const struct movement_event_message *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_encode(state, 3) && ((((zcbor_uint32_put(state, (10))))
	&& (((((*input).timestamp <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input).timestamp))))
	&& ((((*input).movement_event_choice == _movement_event_message_movement_event__movement_event_shake) ? ((zcbor_uint32_put(state, (0))))
	: (((*input).movement_event_choice == _movement_event_message_movement_event__movement_event_tap) ? ((zcbor_uint32_put(state, (1))))
	: (((*input).movement_event_choice == _movement_event_message_movement_event__movement_event_doubletap) ? ((zcbor_uint32_put(state, (2))))
	: (((*input).movement_event_choice == _movement_event_message_movement_event__movement_event_turn90) ? ((zcbor_uint32_put(state, (3))))
	: (((*input).movement_event_choice == _movement_event_message_movement_event__movement_event_turn180) ? ((zcbor_uint32_put(state, (4))))
	: (((*input).movement_event_choice == _movement_event_message_movement_event__movement_event_falling) ? ((zcbor_uint32_put(state, (5))))
	: (((*input).movement_event_choice == _movement_event_message_movement_event__movement_event_impact) ? ((zcbor_uint32_put(state, (6))))
	: false))))))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 3))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_movement_state_message(
		zcbor_state_t *state, const struct movement_state_message *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_encode(state, 3) && ((((zcbor_uint32_put(state, (11))))
	&& (((((*input).timestamp <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input).timestamp))))
	&& ((((*input).movement_state_choice == _movement_state_message_movement_state__movement_state_normal) ? ((zcbor_uint32_put(state, (0))))
	: (((*input).movement_state_choice == _movement_state_message_movement_state__movement_state_tilted) ? ((zcbor_uint32_put(state, (1))))
	: (((*input).movement_state_choice == _movement_state_message_movement_state__movement_state_upsidedown) ? ((zcbor_uint32_put(state, (2))))
	: false))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 3))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_repeated_deviceToCloud_message_union(
		zcbor_state_t *state, const struct deviceToCloud_message_union_ *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((((*input).union_choice == _deviceToCloud_message_union__gnss_message) ? ((encode_gnss_message(state, (&(*input)._gnss_message))))
	: (((*input).union_choice == _deviceToCloud_message_union__button_message) ? ((encode_button_message(state, (&(*input)._button_message))))
	: (((*input).union_choice == _deviceToCloud_message_union__temperature_message) ? ((encode_temperature_message(state, (&(*input)._temperature_message))))
	: (((*input).union_choice == _deviceToCloud_message_union__humidity_message) ? ((encode_humidity_message(state, (&(*input)._humidity_message))))
	: (((*input).union_choice == _deviceToCloud_message_union__air_pressure_message) ? ((encode_air_pressure_message(state, (&(*input)._air_pressure_message))))
	: (((*input).union_choice == _deviceToCloud_message_union__air_quality_message) ? ((encode_air_quality_message(state, (&(*input)._air_quality_message))))
	: (((*input).union_choice == _deviceToCloud_message_union__rsrp_message) ? ((encode_rsrp_message(state, (&(*input)._rsrp_message))))
	: (((*input).union_choice == _deviceToCloud_message_union__battery_message) ? ((encode_battery_message(state, (&(*input)._battery_message))))
	: (((*input).union_choice == _deviceToCloud_message_union__solar_gain_message) ? ((encode_solar_gain_message(state, (&(*input)._solar_gain_message))))
	: (((*input).union_choice == _deviceToCloud_message_union__movement_event_message) ? ((encode_movement_event_message(state, (&(*input)._movement_event_message))))
	: (((*input).union_choice == _deviceToCloud_message_union__movement_state_message) ? ((encode_movement_state_message(state, (&(*input)._movement_state_message))))
	: false)))))))))))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_deviceToCloud_message(
		zcbor_state_t *state, const struct deviceToCloud_message *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_encode(state, 10) && ((zcbor_multi_encode_minmax(1, 10, &(*input).union_count, (zcbor_encoder_t *)encode_repeated_deviceToCloud_message_union, state, (&(*input)._union), sizeof(struct deviceToCloud_message_union_))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 10))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}



int cbor_encode_deviceToCloud_message(
		uint8_t *payload, size_t payload_len,
		const struct deviceToCloud_message *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[6];

	zcbor_new_state(states, sizeof(states) / sizeof(zcbor_state_t), payload, payload_len, 1);

	bool ret = encode_deviceToCloud_message(states, input);

	if (ret && (payload_len_out != NULL)) {
		*payload_len_out = MIN(payload_len,
				(size_t)states[0].payload - (size_t)payload);
	}

	if (!ret) {
		int err = zcbor_pop_error(states);

		zcbor_print("Return error: %d\r\n", err);
		return (err == ZCBOR_SUCCESS) ? ZCBOR_ERR_UNKNOWN : err;
	}
	return ZCBOR_SUCCESS;
}
