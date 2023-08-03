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
#include "zcbor_decode.h"
#include "cloudToDevice_decode.h"

#if DEFAULT_MAX_QTY != 10
#error "The type file was generated with a different default_max_qty than this file"
#endif

static bool decode_led_message(zcbor_state_t *state, struct led_message *result);
static bool decode_repeated_config_message_sampling_period(zcbor_state_t *state, uint32_t *result);
static bool decode_config_message(zcbor_state_t *state, struct config_message *result);
static bool decode_repeated_cloudToDevice_message_union(zcbor_state_t *state, struct cloudToDevice_message_union_ *result);
static bool decode_cloudToDevice_message(zcbor_state_t *state, struct cloudToDevice_message *result);


static bool decode_led_message(
		zcbor_state_t *state, struct led_message *result)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_decode(state) && ((((zcbor_uint32_expect(state, (1))))
	&& ((zcbor_uint32_decode(state, (&(*result).timestamp)))
	&& ((((*result).timestamp <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false)))
	&& ((zcbor_uint32_decode(state, (&(*result).led_red)))
	&& ((((*result).led_red <= UINT8_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false)))
	&& ((zcbor_uint32_decode(state, (&(*result).led_green)))
	&& ((((*result).led_green <= UINT8_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false)))
	&& ((zcbor_uint32_decode(state, (&(*result).led_blue)))
	&& ((((*result).led_blue <= UINT8_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false)))) || (zcbor_list_map_end_force_decode(state), false)) && zcbor_list_end_decode(state))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool decode_repeated_config_message_sampling_period(
		zcbor_state_t *state, uint32_t *result)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_uint32_decode(state, (&(*result))))
	&& ((((*result) <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool decode_config_message(
		zcbor_state_t *state, struct config_message *result)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_decode(state) && ((((zcbor_uint32_expect(state, (2))))
	&& ((zcbor_uint32_decode(state, (&(*result).timestamp)))
	&& ((((*result).timestamp <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false)))
	&& zcbor_present_decode(&((*result).gnss_enable_present), (zcbor_decoder_t *)zcbor_bool_decode, state, (&(*result).gnss_enable))
	&& zcbor_present_decode(&((*result).sampling_period_present), (zcbor_decoder_t *)decode_repeated_config_message_sampling_period, state, (&(*result).sampling_period))) || (zcbor_list_map_end_force_decode(state), false)) && zcbor_list_end_decode(state))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool decode_repeated_cloudToDevice_message_union(
		zcbor_state_t *state, struct cloudToDevice_message_union_ *result)
{
	zcbor_print("%s\r\n", __func__);
	bool int_res;

	bool tmp_result = (((zcbor_union_start_code(state) && (int_res = ((((decode_led_message(state, (&(*result)._led_message)))) && (((*result).union_choice = _cloudToDevice_message_union__led_message), true))
	|| (zcbor_union_elem_code(state) && (((decode_config_message(state, (&(*result)._config_message)))) && (((*result).union_choice = _cloudToDevice_message_union__config_message), true)))), zcbor_union_end_code(state), int_res))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool decode_cloudToDevice_message(
		zcbor_state_t *state, struct cloudToDevice_message *result)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_decode(state) && ((zcbor_multi_decode(1, 10, &(*result).union_count, (zcbor_decoder_t *)decode_repeated_cloudToDevice_message_union, state, (&(*result)._union), sizeof(struct cloudToDevice_message_union_))) || (zcbor_list_map_end_force_decode(state), false)) && zcbor_list_end_decode(state))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}



int cbor_decode_cloudToDevice_message(
		const uint8_t *payload, size_t payload_len,
		struct cloudToDevice_message *result,
		size_t *payload_len_out)
{
	zcbor_state_t states[5];

	zcbor_new_state(states, sizeof(states) / sizeof(zcbor_state_t), payload, payload_len, 1);

	bool ret = decode_cloudToDevice_message(states, result);

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
