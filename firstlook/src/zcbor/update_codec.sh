#!/bin/bash

# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

export HDR="
Copyright (c) 2023 Nordic Semiconductor ASA
SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
"

if [ ! -n "$1" ]; then
    echo "Error: Please provide the folder containing the cddl files"
    exit 1
fi

zcbor code -c $1/deviceToCloud.cddl \
	--default-max-qty 10 -e -s \
	-t deviceToCloud-message \
	--oc src/deviceToCloud_encode.c \
	--oh include/deviceToCloud_encode.h \
	--oht include/deviceToCloud_encode_types.h \
	--file-header "$HDR"
zcbor code -c $1/cloudToDevice.cddl \
	--default-max-qty 10 -d -s \
	-t cloudToDevice-message \
	--oc src/cloudToDevice_decode.c \
	--oh include/cloudToDevice_decode.h \
	--oht include/cloudToDevice_decode_types.h \
	--file-header "$HDR"