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

MFW_201_FILEPATH = "artifacts/mfw_nrf91x1_2.0.1.zip"
DELTA_MFW_BUNDLEID = "MODEM*ad48df2a*mfw_nrf91x1_2.0.1-FOTA-TEST"

WAIT_FOR_FOTA_AVAILABLE = 60 * 2
APP_FOTA_TIMEOUT = 60 * 10


def post_job_and_trigger_fota(t91x_board, bundle_id, fota_type):
    if bundle_id:
        t91x_board.fota.post_fota_job(device_id=DEVICE_ID, fingerprint=FOTADEVICE_FINGERPRINT, bundle_id=bundle_id)
    t91x_board.uart.flush()

    start = time.time()
    while time.time() - start < WAIT_FOR_FOTA_AVAILABLE:
        time.sleep(20)
        try:
            t91x_board.uart.write("zbus button_press\r\n")
            t91x_board.uart.wait_for_str("FOTA Job: ", timeout=30)
            return
        except AssertionError:
            logger.debug(f"{fota_type} fota not available yet, trying again...")

    raise RuntimeError(f"{fota_type} fota not available after {WAIT_FOR_FOTA_AVAILABLE} seconds")


def run_fota_resumption(t91x_board, fota_type):
    logger.debug(f"Testing fota resumption on disconnect for {fota_type} fota")
    t91x_board.uart.wait_for_str("bytes (50%)")

    patterns_lte_offline = ["network: Network connectivity lost"]
    patterns_lte_normal = ["network: Network connectivity established", "transport: Connected to Cloud"]

    # LTE disconnect
    t91x_board.uart.flush()
    t91x_board.uart.write("lte offline\r\n")
    t91x_board.uart.wait_for_str(patterns_lte_offline, timeout=20)

    # LTE reconnect
    t91x_board.uart.flush()
    t91x_board.uart.write("lte normal\r\n")
    t91x_board.uart.wait_for_str(patterns_lte_normal, timeout=120)

    post_job_and_trigger_fota(t91x_board, None, fota_type)  # We don't need bundle_id here

    t91x_board.uart.wait_for_str("fota_download: Refuse fragment, restart with offset")
    t91x_board.uart.wait_for_str("fota_download: Downloading from offset:")

@pytest.fixture
def run_fota_fixture(t91x_board, hex_file):
    def _run_fota(bundleId, fota_type, fotatimeout=APP_FOTA_TIMEOUT, test_fota_resumption=False):
        flash_device(os.path.abspath(hex_file))
        time.sleep(5)
        t91x_board.uart.xfactoryreset()
        reset_device()
        t91x_board.uart.wait_for_str("Connected to Cloud")

        post_job_and_trigger_fota(t91x_board, bundleId, fota_type)

        if test_fota_resumption:
            run_fota_resumption(t91x_board, fota_type)

        t91x_board.uart.wait_for_str("FOTA download finished", timeout=fotatimeout)
        if fota_type == "app":
            t91x_board.uart.wait_for_str("App FOTA update confirmed")
        elif fota_type == "delta":
            t91x_board.uart.wait_for_str("Modem (delta) FOTA complete")
        t91x_board.uart.wait_for_str("Connected to Cloud", "Failed to connect to Cloud after FOTA")
    return _run_fota


@pytest.mark.dut1
@pytest.mark.fota
def test_app_fota(t91x_board, hex_file, run_fota_fixture):
    # Get latest app fota bundle
    results = t91x_board.fota.get_fota_bundles(device_id=DEVICE_ID, fingerprint=FOTADEVICE_FINGERPRINT)
    if not results:
        pytest.fail("Failed to get APP FOTA bundles")
    available_bundles = results["bundles"]
    logger.debug(f"Number of available bundles: {len(available_bundles)}")
    latest_bundle = available_bundles[0]

    logger.debug(f"Latest bundle: {latest_bundle}")

    run_fota_fixture(bundleId=latest_bundle["bundleId"], fota_type="app", test_fota_resumption=True)


@pytest.mark.dut1
@pytest.mark.fota
def test_delta_mfw_fota(t91x_board, hex_file, run_fota_fixture):
    # Flash with mfw201
    flash_device(os.path.abspath(MFW_201_FILEPATH))

    # run_fota(DELTA_MFW_BUNDLEID, hex_file)
    run_fota_fixture(DELTA_MFW_BUNDLEID, "delta")

    # Restore mfw201
    flash_device(os.path.abspath(MFW_201_FILEPATH))
