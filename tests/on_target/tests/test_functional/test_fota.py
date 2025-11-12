##########################################################################################
# Copyright (c) 2024 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
##########################################################################################

import pytest
import os
from utils.flash_tools import flash_device, reset_device
import sys
sys.path.append(os.getcwd())
from utils.logger import get_logger

logger = get_logger()

MFW_FILEPATH = "artifacts/mfw_nrf91x1_2.0.3.zip"
DELTA_MFW_BUNDLEID = "c8443b86-d295-4305-8e39-0ae2731178e1"
FULL_MFW_BUNDLEID = "61cedb8d-0b6f-4684-9150-5aa782c6c8d5"
APP_BUNDLEID = "APP*a82b7076*v2.0.1"

APP_FOTA_TIMEOUT = 60 * 10
FULL_MFW_FOTA_TIMEOUT = 60 * 30

def post_job(t91x_fota, bundle_id, fota_type):
    result = t91x_fota.fota.post_fota_job(type=fota_type, bundle_id=bundle_id)
    if not result:
        pytest.skip("Failed to post FOTA job")
    t91x_fota.uart.flush()
    return


@pytest.fixture
def run_fota_fixture(t91x_fota, hex_file):
    def _run_fota(bundleId, fota_type, fotatimeout=APP_FOTA_TIMEOUT, test_fota_resumption=False):
        flash_device(os.path.abspath(hex_file))
        t91x_fota.uart.xfactoryreset()
        t91x_fota.uart.flush()
        reset_device()
        t91x_fota.uart.wait_for_str("Connected to Cloud")

        post_job(t91x_fota, bundleId, fota_type)

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
    flash_device(os.path.abspath(MFW_FILEPATH))

    try:
        run_fota_fixture(DELTA_MFW_BUNDLEID, "delta")
    finally:
        flash_device(os.path.abspath(MFW_FILEPATH))

@pytest.mark.slow
def test_full_mfw_fota(run_fota_fixture):
    '''
    Test full modem FOTA on nrf9151
    '''
    run_fota_fixture(FULL_MFW_BUNDLEID, "full", FULL_MFW_FOTA_TIMEOUT)
