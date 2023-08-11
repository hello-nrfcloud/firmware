#
# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(led_module_test)

test_runner_generate(src/led_module_test.c)

target_sources(app
  PRIVATE
  src/led_module_test.c
  ../../src/modules/led/led.c
  ../../src/zbus/message_channel.c
)

zephyr_include_directories(${ZEPHYR_BASE}/include/zephyr/)
zephyr_include_directories(${ZEPHYR_BASE}/subsys/testsuite/include)
zephyr_include_directories(../../src/zbus/)
zephyr_include_directories(../../src/zcbor/include/)


target_link_options(app PRIVATE --whole-archive)
# Options that cannot be passed through Kconfig fragments
target_compile_definitions(app PRIVATE
	-DCONFIG_APP_MODULE_LED_LOG_LEVEL=4
)
