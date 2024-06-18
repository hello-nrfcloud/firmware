/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <unity.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include "simple_config_internal.h"


#include <zephyr/fff.h>

DEFINE_FFF_GLOBALS;

int simple_config_callback(const char *key, const struct simple_config_val *val);

/* cJSON functions */
FAKE_VOID_FUNC(cJSON_Init)
FAKE_VOID_FUNC(cJSON_Delete, cJSON *)
FAKE_VOID_FUNC(cJSON_DeleteItemFromObject, cJSON *, const char *)
FAKE_VALUE_FUNC(cJSON *, cJSON_CreateObject)
FAKE_VALUE_FUNC(cJSON *, cJSON_AddStringToObject, cJSON * const, const char * const, const char * const)
FAKE_VALUE_FUNC(cJSON *, cJSON_AddTrueToObject, cJSON * const, const char * const)
FAKE_VALUE_FUNC(cJSON *, cJSON_AddFalseToObject, cJSON * const, const char * const)
FAKE_VALUE_FUNC(cJSON *, cJSON_AddNumberToObject, cJSON * const, const char * const, const double)
FAKE_VALUE_FUNC(cJSON *, cJSON_Parse, const char *)
FAKE_VALUE_FUNC(cJSON_bool, cJSON_IsObject, const cJSON * const)
FAKE_VALUE_FUNC(cJSON *, cJSON_GetObjectItem, const cJSON * const, const char * const)

/* nRF Cloud functions */
FAKE_VALUE_FUNC(int, nrf_cloud_coap_shadow_get, char *, size_t *, bool)

/* fake callback */
FAKE_VALUE_FUNC(int, simple_config_callback, const char *, const struct simple_config_val *);

void setUp(void)
{
	RESET_FAKE(cJSON_Init);
	RESET_FAKE(cJSON_Delete);
	RESET_FAKE(cJSON_DeleteItemFromObject);
	RESET_FAKE(cJSON_CreateObject);
	RESET_FAKE(cJSON_AddStringToObject);
	RESET_FAKE(cJSON_AddTrueToObject);
	RESET_FAKE(cJSON_AddFalseToObject);
	RESET_FAKE(cJSON_AddNumberToObject);
	RESET_FAKE(cJSON_Parse);
	RESET_FAKE(cJSON_IsObject);
	RESET_FAKE(cJSON_GetObjectItem);
	RESET_FAKE(nrf_cloud_coap_shadow_get);
	RESET_FAKE(simple_config_callback);
}

void tearDown(void)
{
	simple_config_clear_queued_configs();
	simple_config_set_callback(NULL);
}

void test_handle_incoming_settings_no_callback(void)
{
	int err = simple_config_handle_incoming_settings(NULL, 0);
	TEST_ASSERT_EQUAL(-EFAULT, err);
}

void test_handle_incoming_settings_no_connection(void)
{
	simple_config_set_callback(simple_config_callback);
	nrf_cloud_coap_shadow_get_fake.return_val = -EACCES;
	int err = simple_config_handle_incoming_settings(NULL, 0);
	TEST_ASSERT_EQUAL(-EACCES, err);
}

void test_handle_incoming_settings_coap_timeout(void)
{
	simple_config_set_callback(simple_config_callback);
	nrf_cloud_coap_shadow_get_fake.return_val = -ETIMEDOUT;
	int err = simple_config_handle_incoming_settings(NULL, 0);
	TEST_ASSERT_EQUAL(-ETIMEDOUT, err);
}

void test_handle_incoming_settings_empty_delta(void)
{
	char buf[COAP_SHADOW_MAX_SIZE] = "{}";
	cJSON dummy = {.type = cJSON_Object};

	simple_config_set_callback(simple_config_callback);
	nrf_cloud_coap_shadow_get_fake.return_val = 0;
	cJSON_Parse_fake.return_val = &dummy;
	cJSON_IsObject_fake.return_val = false;

	int err = simple_config_handle_incoming_settings(buf, sizeof(buf));

	TEST_ASSERT_EQUAL(-ENOENT, err);
}

void test_handle_incoming_settings_rich_delta(void)
{
	char buf[COAP_SHADOW_MAX_SIZE] = "{ \"config\": {\"mystr\": \"foo\", \"mynumber\": 5, \"mytrue\": true, \"myfalse\": false} }";
	/* the main object has the first child node on .child,
	   each child links to the next one with .next */

	cJSON myfalse = {
		.type = cJSON_False,
		.string = "myfalse",
		.valuestring = NULL,
	};
	cJSON mytrue = {
		.type = cJSON_True,
		.string = "mytrue",
		.next = &myfalse,
		.valuestring = NULL,
	};
	cJSON mynumber = {
		.type = cJSON_Number,
		.string = "mynumber",
		.valuedouble = 5,
		.valueint = 5,
		.next = &mytrue,
		.valuestring = NULL,
	};
	cJSON mystr = {
		.type = cJSON_String,
		.string = "mystr",
		.valuestring = "foo",
		.next = &mynumber,
		.valuestring = "\"foo\"",
	};
	cJSON config = {
		.type = cJSON_Object,
		.string = "config",
		.child = &mystr,
	};
	cJSON root = {
		.type = cJSON_Object,
		.child = &config,
	};

	simple_config_set_callback(simple_config_callback);
	nrf_cloud_coap_shadow_get_fake.return_val = 0;
	cJSON_Parse_fake.return_val = &root;
	cJSON_IsObject_fake.return_val = true;
	cJSON_GetObjectItem_fake.return_val = &config;
	cJSON_CreateObject_fake.return_val = &root;

	cJSON_AddStringToObject_fake.return_val = &mystr;
	cJSON_AddTrueToObject_fake.return_val = &mytrue;
	cJSON_AddFalseToObject_fake.return_val = &myfalse;
	cJSON_AddNumberToObject_fake.return_val = &mynumber;

	int err = simple_config_handle_incoming_settings(buf, sizeof(buf));

	TEST_ASSERT_EQUAL(0, err);
	TEST_ASSERT_EQUAL(4, simple_config_callback_fake.call_count);
}


void test_simple_config_set_no_key(void)
{
	struct simple_config_val val = {.type = SIMPLE_CONFIG_VAL_BOOL, .val._bool=true};
	int err = simple_config_set(NULL, &val);

	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void test_simple_config_set_empty_key(void)
{
	struct simple_config_val val = {.type = SIMPLE_CONFIG_VAL_BOOL, .val._bool=true};
	int err = simple_config_set("", &val);

	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void test_simple_config_set_invalid_type(void)
{
	struct simple_config_val val = {.type = -1};
	int err = simple_config_set("", &val);

	TEST_ASSERT_EQUAL(-EINVAL, err);
}



/* It is required to be added to each test. That is because unity's
 * main may return nonzero, while zephyr's main currently must
 * return 0 in all cases (other values are reserved).
 */
extern int unity_main(void);

int main(void)
{
	/* use the runner from test_runner_generate() */
	(void)unity_main();

	return 0;
}
