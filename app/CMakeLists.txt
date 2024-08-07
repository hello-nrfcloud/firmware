#
# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mqtt)

# Include files that are common for all modules
add_subdirectory(src/common)

# Include mandatory module source folders
add_subdirectory(src/modules/trigger)
add_subdirectory(src/modules/battery)
add_subdirectory(src/modules/network)
add_subdirectory(src/modules/transport)
add_subdirectory(src/modules/location)
add_subdirectory(src/modules/app)
add_subdirectory(src/modules/fota)
add_subdirectory(src/modules/environmental)

# Optional modules
add_subdirectory_ifdef(CONFIG_APP_LED src/modules/led)
add_subdirectory_ifdef(CONFIG_APP_MEMFAULT src/modules/memfault)
add_subdirectory_ifdef(CONFIG_APP_SHELL src/modules/shell)

# Include Memfault configuration folder
zephyr_include_directories(config)
