/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include "cJSON.h"
#include "cJSON_os.h"
#include <net/nrf_cloud.h>
#include <net/nrf_cloud_coap.h>
#include <zephyr/logging/log.h>
#include <stdio.h>

#include "simple_config_internal.h"

LOG_MODULE_REGISTER(simple_config, CONFIG_SIMPLE_CONFIG_LOG_LEVEL);


/* cJSON object holding all the pending settings entries. Lives forever. These settings are already
 * applied and should be reported to cloud. */
static cJSON *queued_configs;

static simple_config_callback_t callback;
void simple_config_set_callback(simple_config_callback_t cb)
{
	cJSON_Init();
	callback = cb;
}

int simple_config_handle_incoming_settings(char *buf, size_t buf_len)
{
	int err = 0;
	cJSON *root_obj = NULL;
	cJSON *config_obj = NULL;
	cJSON *child = NULL;

	/* initially, request the full shadow */
	static bool request_delta;

	if (callback == NULL) {
		LOG_ERR("callback is not set up, settings cannot be applied!");
		return -EFAULT;
	}

	LOG_INF("Checking for shadow delta...");
	err = nrf_cloud_coap_shadow_get(buf, buf_len, request_delta);
	if (err == -EACCES) {
		LOG_DBG("Not connected yet.");
		return err;
	} else if (err) {
		LOG_ERR("Failed to request shadow delta: %d", err);
		return err;
	}
	LOG_DBG("Shadow: len:%zd, %s", strnlen(buf, buf_len), buf);
	request_delta = true;

	if (strnlen(buf, buf_len) == 0) {
		LOG_DBG("No shadow delta available");
		return 0;
	}

	/* try to parse the json string into an object */
	root_obj = cJSON_Parse(buf);
	if (root_obj == NULL) {
		LOG_ERR("Shadow delta could not be parsed");
		return -EINVAL;
	}
	if (!cJSON_IsObject(root_obj)) {
		LOG_ERR("Shadow delta is not an object");
		return -ENOENT;
	}

	/* try to get the 'config' entry of the json object */
	config_obj = cJSON_GetObjectItem(root_obj, "config");
	if (!config_obj) {
		cJSON_Delete(root_obj);
		return -ENOENT;
	}
	if (!cJSON_IsObject(config_obj)) {
		cJSON_Delete(root_obj);
		LOG_ERR("config is not an object");
		return -ENOENT;
	}

	/* iterate over settings entries */
	cJSON_ArrayForEach(child, config_obj)
	{
		struct simple_config_val val;

		/* match cJSON entries to settings struct */
		if (child->type == cJSON_String) {
			val.type = SIMPLE_CONFIG_VAL_STRING;
			val.val._str = child->valuestring;
		} else if (child->type == cJSON_Number) {
			val.type = SIMPLE_CONFIG_VAL_DOUBLE;
			val.val._double = child->valuedouble;
		} else if (child->type == cJSON_True) {
			val.type = SIMPLE_CONFIG_VAL_BOOL;
			val.val._bool = true;
		} else if (child->type == cJSON_False) {
			val.type = SIMPLE_CONFIG_VAL_BOOL;
			val.val._bool = false;
		} else {
			LOG_ERR("config entry %s has unsupported type!", child->string);
			/* reject entry */
			continue;
		}

		/* inform app about settings change */
		if (!callback(child->string, &val)) {
			/* on success, apply this to the queue */
			simple_config_set(child->string, &val);
		}
	};

	cJSON_Delete(root_obj);
	return 0;
}

cJSON *simple_config_construct_settings_obj(void)
{
	if (simple_config_init_queued_configs()) {
		return NULL;
	}

	int i = 0;

	switch (i) {
	case 0:
		break;
	}

	cJSON *root_obj = cJSON_CreateObject();

	if (!root_obj) {
		LOG_ERR("couldn't create root object!");
		return NULL;
	}

	if (cJSON_AddItemToObjectCS(root_obj, "config", queued_configs)) {
		queued_configs = cJSON_CreateObject();
		if (!queued_configs) {
			LOG_ERR("couldn't create new queued_configs object!");
		}
	} else {
		LOG_ERR("couldn't attach queued_configs");
		cJSON_Delete(root_obj);
		return NULL;
	}

	return root_obj;
}

int simple_config_update(void)
{
	int err = 0;
	cJSON *root_obj = NULL;
	char buf[COAP_SHADOW_MAX_SIZE] = {0};

	err = simple_config_handle_incoming_settings(buf, sizeof(buf));
	if (err) {
		LOG_INF("handling incoming settings failed: %d", err);
		if (err == -EFAULT || err == -EACCES) {
			return err;
		}
	}

	root_obj = simple_config_construct_settings_obj();
	if (!root_obj) {
		LOG_INF("coudn't construct settings object: %d", err);
		return -EIO;
	}

	if (cJSON_PrintPreallocated(root_obj, buf, sizeof(buf), false)) {
		LOG_DBG("sending settings: %s", buf);
		err = nrf_cloud_coap_shadow_state_update(buf);
		if (err) {
			LOG_ERR("nrf_cloud_coap_shadow_state_update failed: %d", err);
		}
	} else {
		LOG_ERR("rendering delta response failed!");
		err = -EIO;
	}

	cJSON_Delete(root_obj);

	return err;
}

int simple_config_init_queued_configs(void)
{
	if (queued_configs == NULL) {
		LOG_DBG("initilizing [queued_configs]");
		queued_configs = cJSON_CreateObject();
	}
	if (queued_configs == NULL) {
		LOG_ERR("[queued_configs] couldn't be created!");
		return -ENOMEM;
	}
	return 0;
}

void simple_config_clear_queued_configs(void)
{
	if (queued_configs) {
		cJSON_Delete(queued_configs);
		queued_configs = NULL;
	}
}

int simple_config_set(const char *key, const struct simple_config_val *val)
{
	cJSON *child = NULL;

	if (key == NULL || key[0] == '\0' ||
	    (val->type != SIMPLE_CONFIG_VAL_STRING && val->type != SIMPLE_CONFIG_VAL_BOOL &&
	     val->type != SIMPLE_CONFIG_VAL_DOUBLE)) {
		return -EINVAL;
	}

	if (simple_config_init_queued_configs()) {
		return -ENOMEM;
	}

	/* delete config item if it already existed */
	cJSON_DeleteItemFromObject(queued_configs, key);

	if (val->type == SIMPLE_CONFIG_VAL_STRING) {
		child = cJSON_AddStringToObject(queued_configs, key, val->val._str);
	} else if (val->type == SIMPLE_CONFIG_VAL_BOOL) {
		if (val->val._bool) {
			child = cJSON_AddTrueToObject(queued_configs, key);
		} else {
			child = cJSON_AddFalseToObject(queued_configs, key);
		}
	} else if (val->type == SIMPLE_CONFIG_VAL_DOUBLE) {
		child = cJSON_AddNumberToObject(queued_configs, key, val->val._double);
	}

	if (child == NULL) {
		LOG_ERR("couldn't add config item for key [%s]", key);
		return -ENOMEM;
	}

	return 0;
}
