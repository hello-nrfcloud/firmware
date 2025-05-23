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

MFW_202_FILEPATH = "artifacts/mfw_nrf91x1_2.0.2.zip"
DELTA_MFW_BUNDLEID = "MODEM*3471f88e*mfw_nrf91x1_2.0.2-FOTA-TEST"
FULL_MFW_BUNDLEID = "MDM_FULL*124c2b20*mfw_nrf91x1_full_2.0.2"
APP_BUNDLEID = "APP*a82b7076*v2.0.1"

APP_FOTA_TIMEOUT = 60 * 10
FULL_MFW_FOTA_TIMEOUT = 60 * 30

def post_job(t91x_fota, bundle_id, fota_type):
    result = t91x_fota.fota.post_fota_job(type=fota_type, bundle_id=bundle_id)
    if not result:
        pytest.skip("Failed to post FOTA job")
    t91x_fota.uart.flush()
    return

def run_fota_resumption(t91x_fota, fota_type):
    t91x_fota.uart.wait_for_str("50%")
    logger.debug(f"Testing fota resumption on disconnect for {fota_type} fota")

    patterns_lte_offline = ["network: Network connectivity lost"]
    patterns_lte_normal = ["network: Network connectivity established"]

    # LTE disconnect
    t91x_fota.uart.flush()
    t91x_fota.uart.write("lte offline\r\n")
    t91x_fota.uart.wait_for_str(patterns_lte_offline, timeout=20)

    # LTE reconnect
    t91x_fota.uart.flush()
    t91x_fota.uart.write("lte normal\r\n")
    t91x_fota.uart.wait_for_str(patterns_lte_normal, timeout=120)

    t91x_fota.uart.wait_for_str("fota_download: Refuse fragment, restart with offset")
    t91x_fota.uart.wait_for_str("fota_download: Downloading from offset:")

@pytest.fixture
def run_fota_fixture(t91x_fota, hex_file):
    def _run_fota(bundleId, fota_type, fotatimeout=APP_FOTA_TIMEOUT, test_fota_resumption=False):
        flash_device(os.path.abspath(hex_file))
        t91x_fota.uart.xfactoryreset()
        t91x_fota.uart.flush()
        reset_device()
        t91x_fota.uart.wait_for_str("Connected to Cloud")

        post_job(t91x_fota, bundleId, fota_type)

        # if test_fota_resumption:
        #     run_fota_resumption(t91x_fota, fota_type)

        t91x_fota.uart.flush()
        t91x_fota.uart.wait_for_str("FOTA download finished", timeout=fotatimeout)
        if fota_type == "app":
            t91x_fota.uart.wait_for_str("App FOTA update confirmed")
        elif fota_type == "delta":
            t91x_fota.uart.wait_for_str("Modem (delta) FOTA complete")
        elif fota_type == "full":
            t91x_fota.uart.wait_for_str("FMFU finished")
        t91x_fota.uart.wait_for_str("Connected to Cloud", "Failed to connect to Cloud after FOTA")
        t91x_fota.uart.wait_for_str("No pending FOTA job")

    return _run_fota

def test_app_fota(run_fota_fixture):
    '''
    Test application FOTA on nrf9151
    '''
    run_fota_fixture(bundleId=APP_BUNDLEID, fota_type="app", test_fota_resumption=True)


def test_delta_mfw_fota(run_fota_fixture):
    '''
    Test delta modem FOTA on nrf9151
    '''
    # Flash with mfw202
    flash_device(os.path.abspath(MFW_202_FILEPATH))

    try:
        # run_fota(DELTA_MFW_BUNDLEID, hex_file)
        run_fota_fixture(DELTA_MFW_BUNDLEID, "delta")
    finally:
        # Restore mfw202, no matter if test pass/fails
        flash_device(os.path.abspath(MFW_202_FILEPATH))

@pytest.mark.slow
def test_full_mfw_fota(run_fota_fixture):
    '''
    Test full modem FOTA on nrf9151
    '''
    run_fota_fixture(FULL_MFW_BUNDLEID, "full", FULL_MFW_FOTA_TIMEOUT)
