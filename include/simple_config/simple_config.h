/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef SIMPLE_CONFIG_H__
#define SIMPLE_CONFIG_H__



#include <zephyr/kernel.h>
#include "cJSON.h"

#define COAP_SHADOW_MAX_SIZE 512

/**
 * @brief Simple Config configuration value
 * @note Can be either a string, a boolean or a double.
 *       Check `type` first and use the corresponding `val` entry.
 *       When copying a string value, make sure to also copy the string,
 *       since the original will be free'd.
 *
 */
struct simple_config_val {
	enum {
		SIMPLE_CONFIG_VAL_STRING,
		SIMPLE_CONFIG_VAL_BOOL,
		SIMPLE_CONFIG_VAL_DOUBLE,
	} type;
	union {
		const char *_str;
		bool _bool;
		double _double;
	} val;
};

/**
 * @brief Callback type for simple_config_set_callback.
 * @param[in] key key of the incoming config element
 * @param[in] val value of the incoming config element
 *
 * @return 0 to accept the configuration, negative error code to reject it.
 */
typedef int (*simple_config_callback_t)(const char *key, const struct simple_config_val *val);

/**
 * @brief Check for new configuration from the cloud and send pending configs.
 *
 * @return int 0 on success,
 *             -EIO if pending configs couldn't be sent,
 *             -EFAULT if callback is missing,
 *             -EACCES on network failure.
 */
int simple_config_update(void);

/**
 * @brief Set a configuration entry.
 *        This adds it to an internal queue of pending configs to be sent to cloud.
 *
 * @param[out] key key of the config element to be set
 * @param[out] val value of the config element to be set
 * @return int 0 on success,
 *             -EINVAL on invalid arguments,
 *             -ENOMEM if running out of heap space.
 */
int simple_config_set(const char *key, const struct simple_config_val *val);

/**
 * @brief Set callback to be notified about incoming configs from cloud.
 *
 * @param cb callback function
 */
void simple_config_set_callback(simple_config_callback_t cb);


#endif /* SIMPLE_CONFIG_H__ */
