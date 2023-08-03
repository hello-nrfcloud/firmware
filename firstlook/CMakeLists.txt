#
# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(firstlook)

# Include files that are common for all modules
add_subdirectory(src/zbus)
add_subdirectory(src/zcbor)

# Include mandatory module source folders
add_subdirectory(src/modules/trigger)
add_subdirectory(src/modules/sampler)
add_subdirectory(src/modules/network)
add_subdirectory(src/modules/transport)
add_subdirectory(src/modules/led)
add_subdirectory(src/modules/gas_sensor)
add_subdirectory(src/modules/location)
add_subdirectory(src/modules/movement)
add_subdirectory(src/modules/error)