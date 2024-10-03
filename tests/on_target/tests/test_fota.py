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

MFW_201_FILEPATH = "artifacts/mfw_nrf91x1_2.0.1.zip"
DELTA_MFW_BUNDLEID = "MODEM*ad48df2a*mfw_nrf91x1_2.0.1-FOTA-TEST"
FULL_MFW_BUNDLEID = "MDM_FULL*bdd24c80*mfw_nrf91x1_full_2.0.1"

APP_FOTA_TIMEOUT = 60 * 10
FULL_MFW_FOTA_TIMEOUT = 60 * 30

def post_job(t91x_board, bundle_id, fota_type):
    result = t91x_board.fota.post_fota_job(type=fota_type, bundle_id=bundle_id)
    job_id = result["id"]
    if not result:
        pytest.skip("Failed to post FOTA job")
    t91x_board.uart.flush()
    return job_id

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

    t91x_board.uart.wait_for_str("fota_download: Refuse fragment, restart with offset")
    t91x_board.uart.wait_for_str("fota_download: Downloading from offset:")

@pytest.fixture
def run_fota_fixture(t91x_board, hex_file):
    def _run_fota(bundleId, fota_type, fotatimeout=APP_FOTA_TIMEOUT, test_fota_resumption=False):
        flash_device(os.path.abspath(hex_file))
        time.sleep(10)
        t91x_board.uart.xfactoryreset()
        t91x_board.uart.flush()
        reset_device()
        t91x_board.uart.wait_for_str("Connected to Cloud")

        job_id = post_job(t91x_board, bundleId, fota_type)

        # if test_fota_resumption:
        #     run_fota_resumption(t91x_board, fota_type)

        t91x_board.uart.flush()
        t91x_board.uart.wait_for_str("FOTA download finished", timeout=fotatimeout)
        if fota_type == "app":
            t91x_board.uart.wait_for_str("App FOTA update confirmed")
        elif fota_type == "delta":
            t91x_board.uart.wait_for_str("Modem (delta) FOTA complete")
        elif fota_type == "full":
            t91x_board.uart.wait_for_str("FMFU finished")
        t91x_board.uart.wait_for_str("Connected to Cloud", "Failed to connect to Cloud after FOTA")

        # Check the job status on cloud
        number_of_attempts = 30
        sleep_time = 30
        for i in range(number_of_attempts):
            logger.debug(f"Checking FOTA job status in cloud, attempt {i+1}")
            time.sleep(sleep_time)
            jobs = t91x_board.fota.list_fota_jobs()
            job = next((job for job in jobs if job['id'] == job_id), None)
            if job:
                status = job['status']
                if status == 'SUCCEEDED':
                    logger.info("FOTA job succeeded reported to cloud")
                    break
                else:
                    logger.warning(f"FOTA job status in cloud is {status}, retrying in {sleep_time} seconds")
                    continue
            else:
                pytest.fail("FOTA job not found in the list of jobs")
        else:
            pytest.fail(f"FOTA job status not SUCCEEDED after {number_of_attempts * sleep_time} seconds")

    return _run_fota


@pytest.mark.dut1
@pytest.mark.fota
def test_app_fota(t91x_board, hex_file, run_fota_fixture):
    # Get latest app fota bundle
    results = t91x_board.fota.get_fota_bundles()
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


@pytest.mark.dut1
@pytest.mark.fullmfw_fota
def test_full_mfw_fota(t91x_board, hex_file, run_fota_fixture):
    run_fota_fixture(FULL_MFW_BUNDLEID, "full", FULL_MFW_FOTA_TIMEOUT)
