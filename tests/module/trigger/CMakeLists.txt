#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(trigger_module_test)

test_runner_generate(src/main.c)

target_sources(app
  PRIVATE
  src/main.c
  ../../../app/src/modules/trigger/trigger.c
  ../../../app/src/common/message_channel.c
)

zephyr_include_directories(${ZEPHYR_BASE}/include/zephyr/)
zephyr_include_directories(${ZEPHYR_BASE}/subsys/testsuite/include)
zephyr_include_directories(../../../app/src/common)

# Options that cannot be passed through Kconfig fragments
target_compile_definitions(app PRIVATE
	-DCONFIG_APP_PAYLOAD_CHANNEL_BUFFER_MAX_SIZE=100
	-DCONFIG_APP_TRIGGER_LOG_LEVEL=4
	-DCONFIG_APP_TRIGGER_THREAD_STACK_SIZE=1024
	-DCONFIG_APP_TRIGGER_MESSAGE_QUEUE_SIZE=5
	-DCONFIG_APP_TRIGGER_EXEC_TIME_SECONDS_MAX=1
	-DCONFIG_APP_TRIGGER_WATCHDOG_TIMEOUT_SECONDS=2
	-DCONFIG_NET_MGMT_EVENT
	-DCONFIG_APP_TRIGGER_TIMEOUT_SECONDS=3600
	-DCONFIG_FREQUENT_POLL_DURATION_INTERVAL_SEC=600
)
