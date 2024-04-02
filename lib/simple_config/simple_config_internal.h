/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <simple_config/simple_config.h>

size_t strnlen(const char *, size_t);
int simple_config_init_queued_configs(void);
void simple_config_clear_queued_configs(void);
int simple_config_handle_incoming_settings(char *buf, size_t buf_len);
cJSON *simple_config_construct_settings_obj(void);
