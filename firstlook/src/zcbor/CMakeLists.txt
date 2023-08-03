#
# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

target_include_directories(app PRIVATE include)

target_sources(app PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/src/cloudToDevice_decode.c
               ${CMAKE_CURRENT_SOURCE_DIR}/src/deviceToCloud_encode.c
)
