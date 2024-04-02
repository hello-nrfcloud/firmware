#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(simple_config_test)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})

test_runner_generate(src/main.c)

target_sources(app
  PRIVATE
  ../../../lib/simple_config/simple_config.c
)
zephyr_include_directories(${ZEPHYR_NRFXLIB_MODULE_DIR}/nrf_modem/include/)
zephyr_include_directories(${ZEPHYR_NRF_MODULE_DIR}/../modules/lib/cjson/)
zephyr_include_directories(${ZEPHYR_NRF_MODULE_DIR}/modules/cjson/include/)
zephyr_include_directories(${ZEPHYR_NRF_MODULE_DIR}/include/net/)
zephyr_include_directories(${ZEPHYR_BASE}/subsys/testsuite/include)
zephyr_include_directories(../../../lib/simple_config/)
zephyr_include_directories(${ZEPHYR_NRF_MODULE_DIR}/subsys/net/lib/nrf_cloud/include)


target_compile_options(app
  PRIVATE
  -DCONFIG_SIMPLE_CONFIG_LOG_LEVEL_DBG=y
  -DCONFIG_SIMPLE_CONFIG_LOG_LEVEL=4
)
