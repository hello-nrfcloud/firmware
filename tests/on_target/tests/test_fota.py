##########################################################################################
# Copyright (c) 2024 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
##########################################################################################

import pytest
import time
import os
from utils.flash_tools import flash_device, reset_device
import sys
sys.path.append(os.getcwd())
from utils.logger import get_logger

logger = get_logger()

FOTADEVICE_IMEI = os.getenv('IMEI')
FOTADEVICE_FINGERPRINT = os.getenv('FINGERPRINT')
DEVICE_ID = f"oob-{FOTADEVICE_IMEI}"

WAIT_FOR_FOTA_AVAILABLE = 60
APP_FOTA_TIMEOUT = 60 * 10

@pytest.mark.dut1
def test_app_fota(t91x_board, hex_file):
    flash_device(os.path.abspath(hex_file))
    time.sleep(5)
    t91x_board.uart.xfactoryreset()
    reset_device()

    t91x_board.uart.wait_for_str("Connected to Cloud")

    results = t91x_board.fota.get_fota_bundles(device_id=DEVICE_ID, fingerprint=FOTADEVICE_FINGERPRINT)
    if not results:
        pytest.fail("Failed to get FOTA bundles")
    available_bundles = results["bundles"]
    logger.debug(f"Number of available bundles: {len(available_bundles)}")
    latest_bundle = available_bundles[0]

    logger.debug(f"Latest bundle: {latest_bundle}")

    t91x_board.uart.flush()

    t91x_board.fota.post_fota_job(device_id=DEVICE_ID, fingerprint=FOTADEVICE_FINGERPRINT, bundle_id=latest_bundle["bundleId"])

    time.sleep(WAIT_FOR_FOTA_AVAILABLE)
    t91x_board.uart.write("zbus button_press\r\n")

    t91x_board.uart.wait_for_str("nrf_cloud_fota_poll: FOTA download finished", timeout=APP_FOTA_TIMEOUT)
    t91x_board.uart.wait_for_str("Connected to Cloud")
