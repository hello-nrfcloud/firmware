/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/smf.h>
#include <zephyr/task_wdt/task_wdt.h>
#include <net/nrf_cloud.h>
#include <net/nrf_cloud_coap.h>
#include <app_version.h>

#include "modules_common.h"
#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(bluetooth, CONFIG_APP_BLUETOOTH_LOG_LEVEL);

BUILD_ASSERT(CONFIG_APP_BLUETOOTH_WATCHDOG_TIMEOUT_SECONDS > CONFIG_APP_BLUETOOTH_EXEC_TIME_SECONDS_MAX,
	     "Watchdog timeout must be greater than maximum execution time");

/* Register subscriber */
ZBUS_MSG_SUBSCRIBER_DEFINE(bluetooth);

#define MAX_MSG_SIZE (MAX(sizeof(enum trigger_type), sizeof(enum cloud_status)))

/* Forward declarations */
static const struct smf_state states[];

static void state_running_entry(void *o);
static void state_running_run(void *o);

/* Defining the hierarchical bluetooth module states:
 *
 *   STATE_RUNNING: The bluetooth module has started and is running
 */
enum cloud_module_state {
	STATE_RUNNING,
};

/* Construct state table */
static const struct smf_state states[] = {
	[STATE_RUNNING] =
		SMF_CREATE_STATE(state_running_entry, state_running_run, NULL, NULL, NULL),
};

/* User defined state object.
 * Used to transfer data between state changes.
 */
static struct s_object {
	/* This must be first */
	struct smf_ctx ctx;

	/* Buffer for last zbus message */
	uint8_t msg_buf[MAX_MSG_SIZE];

	/* Last channel type that a message was received on */
	const struct zbus_channel *chan;

} s_obj;

/* Zephyr State Machine Framework handlers */

/* Handler for STATE_RUNNING */

static void state_running_entry(void *o)
{
	ARG_UNUSED(o);

	LOG_WRN("%s", __func__);
}

static void state_running_run(void *o)
{
	ARG_UNUSED(o);

	LOG_WRN("%s", __func__);
}

static void task_wdt_callback(int channel_id, void *user_data)
{
	LOG_ERR("Watchdog expired, Channel: %d, Thread: %s",
		channel_id, k_thread_name_get((k_tid_t)user_data));

	SEND_FATAL_ERROR_WATCHDOG_TIMEOUT();
}

static void bluetooth_task(void)
{
	int err;
	int task_wdt_id;
	const uint32_t wdt_timeout_ms = (CONFIG_APP_BLUETOOTH_WATCHDOG_TIMEOUT_SECONDS * MSEC_PER_SEC);
	const uint32_t execution_time_ms = (CONFIG_APP_BLUETOOTH_EXEC_TIME_SECONDS_MAX * MSEC_PER_SEC);
	const k_timeout_t zbus_wait_ms = K_MSEC(wdt_timeout_ms - execution_time_ms);

	LOG_WRN("Bluetooth module task started");

	task_wdt_id = task_wdt_add(wdt_timeout_ms, task_wdt_callback, (void *)k_current_get());

	/* Initialize the state machine to STATE_RUNNING, which will also run its entry function */
	STATE_SET_INITIAL(STATE_RUNNING);

	while (true) {
		err = task_wdt_feed(task_wdt_id);
		if (err) {
			LOG_ERR("task_wdt_feed, error: %d", err);
			SEND_FATAL_ERROR();

			return;
		}

		err = zbus_sub_wait_msg(&bluetooth, &s_obj.chan, s_obj.msg_buf, zbus_wait_ms);
		if (err == -ENOMSG) {
			continue;
		} else if (err) {
			LOG_ERR("zbus_sub_wait_msg, error: %d", err);
			SEND_FATAL_ERROR();

			return;
		}

		err = STATE_RUN();
		if (err) {
			LOG_ERR("STATE_RUN(), error: %d", err);
			SEND_FATAL_ERROR();

			return;
		}
	}
}

K_THREAD_DEFINE(bluetooth_task_id,
		CONFIG_APP_BLUETOOTH_THREAD_STACK_SIZE,
		bluetooth_task, NULL, NULL, NULL, K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);
