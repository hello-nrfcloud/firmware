/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/shell/shell.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <date_time.h>
#include <zephyr/task_wdt/task_wdt.h>

#include "message_channel.h"

LOG_MODULE_REGISTER(shell, CONFIG_APP_SHELL_LOG_LEVEL);

/* Register subscriber */
ZBUS_SUBSCRIBER_DEFINE(shell, CONFIG_APP_SHELL_MESSAGE_QUEUE_SIZE);

enum zbus_test_type {
	PING,
};

ZBUS_CHAN_DECLARE(ZBUS_TEST_CHAN);
ZBUS_CHAN_DEFINE(ZBUS_TEST_CHAN,
		 enum zbus_test_type,
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS(shell),
		 ZBUS_MSG_INIT(0)
);

static int cmd_zbus_ping(const struct shell *sh, size_t argc,
                         char **argv)
{
	int err;
	enum zbus_test_type test_type;
	
	ARG_UNUSED(sh);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	test_type = PING;

	err = zbus_chan_pub(&ZBUS_TEST_CHAN, &test_type, K_SECONDS(1));
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
	}

	return 0;
}

static int cmd_button_press(const struct shell *sh, size_t argc,
                         char **argv)
{
	int err;
	uint8_t button_number = 1;

	ARG_UNUSED(sh);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	LOG_DBG("Button 1 pressed!");

	err = zbus_chan_pub(&BUTTON_CHAN, &button_number, K_SECONDS(1));
	if (err) {
			LOG_ERR("zbus_chan_pub, error: %d", err);
			return 1;
	}

	return 0;
}

static int cmd_publish_on_payload_chan(const struct shell *sh, size_t argc,
                           char **argv)
{
	ARG_UNUSED(sh);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	LOG_INF("Not implemented yet!");

	return 0;
}


static void task_wdt_callback(int channel_id, void *user_data)
{
	LOG_ERR("Watchdog expired, Channel: %d, Thread: %s",
		channel_id, k_thread_name_get((k_tid_t)user_data));

	SEND_FATAL_ERROR_WATCHDOG_TIMEOUT();
}


/* Handle messages from the message queue.
 * Returns 0 if the message was handled successfully, otherwise an error code.
 */
static int handle_message(const struct zbus_channel *chan)
{
	int err;

	if (&ZBUS_TEST_CHAN == chan) {
		enum zbus_test_type test_type;

		err = zbus_chan_read(&ZBUS_TEST_CHAN, &test_type, K_FOREVER);
		if (err) {
			LOG_ERR("zbus_chan_read, error: %d", err);
			return err;
		}

		if (test_type == PING) {
			LOG_INF("pong");
		}
	}

	return 0;
}

static void shell_task(void)
{
	int err;
	const struct zbus_channel *chan;
	int task_wdt_id;
	const uint32_t wdt_timeout_ms = (CONFIG_APP_SHELL_WATCHDOG_TIMEOUT_SECONDS * MSEC_PER_SEC);
	const uint32_t execution_time_ms = (CONFIG_APP_SHELL_EXEC_TIME_SECONDS_MAX * MSEC_PER_SEC);
	const k_timeout_t zbus_wait_ms = K_MSEC(wdt_timeout_ms - execution_time_ms);

	LOG_DBG("Shell module task started");

	task_wdt_id = task_wdt_add(wdt_timeout_ms, task_wdt_callback, (void *)k_current_get());

	while (true) {
		err = task_wdt_feed(task_wdt_id);
		if (err) {
			LOG_ERR("task_wdt_feed, error: %d", err);
			SEND_FATAL_ERROR();
			return;
		}

		err = zbus_sub_wait(&shell, &chan, zbus_wait_ms);
		if (err == -EAGAIN) {
			continue;
		} else if (err) {
			LOG_ERR("zbus_sub_wait, error: %d", err);
			SEND_FATAL_ERROR();
			return;
		}

		err = handle_message(chan);
		if (err) {
			LOG_ERR("handle_message, error: %d", err);
			SEND_FATAL_ERROR();
			return;
		}
	}

}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_zbus_publish,
				SHELL_CMD(payload_chan,   NULL, "Publish on payload channel", cmd_publish_on_payload_chan),
				SHELL_SUBCMD_SET_END
		);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_zbus,
				SHELL_CMD(ping,   NULL, "Ping command.", cmd_zbus_ping),
				SHELL_CMD(button_press,   NULL, "Button press command.", cmd_button_press),
				SHELL_CMD(publish, &sub_zbus_publish, "Publish on a zbus channel", NULL),
				SHELL_SUBCMD_SET_END
		);

SHELL_CMD_REGISTER(zbus, &sub_zbus, "Zbus shell", NULL);

K_THREAD_DEFINE(shell_task_id,
		CONFIG_APP_SHELL_THREAD_STACK_SIZE,
		shell_task, NULL, NULL, NULL, 3, 0, 0);
