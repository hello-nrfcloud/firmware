##########################################################################################
# Copyright (c) 2024 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
##########################################################################################

import os
import sys
import time

import pytest

sys.path.append(os.getcwd())
from utils.att_release import download_latest_standard_att_dfu_zip
from utils.flash_tools import (
    dfu_device,
    flash_device,
    get_first_artifact_match,
    pyocd_flash_device,
    recover_device,
)
from utils.logger import get_logger
from utils.uart import Uart, wait_until_uart_available
from tests.conftest import get_uarts

logger = get_logger()

pytestmark = pytest.mark.slow

NRF53_NET_HEX_FILE = get_first_artifact_match(
    "artifacts/connectivity-bridge-*-thingy91x-nrf53-net.hex"
)
NRF53_APP_HEX_FILE = get_first_artifact_match(
    "artifacts/connectivity-bridge-*-thingy91x-nrf53-app.hex"
)
OOB_NRF91_DFU_ZIP = get_first_artifact_match(
    "artifacts/hello.nrfcloud.com-*-thingy91x-nrf91-dfu.zip"
)
NRF91_BOOTLOADER = "artifacts/nrf91-bl-v2.hex"

SEGGER_NRF53 = os.getenv("SEGGER_NRF53")
CONN_BRIDGE_SERIAL = f"THINGY91X_{os.getenv('UART_ID_DUT_2')}"


@pytest.fixture(scope="module")
def att_dfu_zip():
    try:
        return download_latest_standard_att_dfu_zip()
    except RuntimeError as err:
        pytest.fail(str(err))


@pytest.fixture(scope="function")
def uart():
    all_uarts = get_uarts(CONN_BRIDGE_SERIAL)
    if not all_uarts:
        pytest.fail("No UARTs found")
    uart = Uart(all_uarts[0])
    yield uart
    uart.stop()


def test_01_setup_nrf53():
    """
    Flash connectivity bridge on nRF53 for Thingy:91 X serial DFU.
    """
    recover_device(serial=SEGGER_NRF53, core="Network")
    recover_device(serial=SEGGER_NRF53, core="Application")
    flash_device(
        hexfile=NRF53_NET_HEX_FILE,
        serial=SEGGER_NRF53,
        extra_args=[
            "--core",
            "Network",
            "--options",
            "reset=RESET_NONE,chip_erase_mode=ERASE_ALL,verify=VERIFY_NONE",
        ],
    )
    flash_device(
        hexfile=NRF53_APP_HEX_FILE,
        serial=SEGGER_NRF53,
        extra_args=[
            "--options",
            "reset=RESET_SYSTEM,chip_erase_mode=ERASE_ALL,verify=VERIFY_NONE",
        ],
    )


def test_02_setup_oob_nrf91():
    """
    Program MCUboot and OOB (Hello nRF Cloud) application on nRF91.
    """
    if not OOB_NRF91_DFU_ZIP:
        pytest.fail("OOB nRF91 DFU artifact not found")
    wait_until_uart_available(CONN_BRIDGE_SERIAL)
    try:
        pyocd_flash_device(serial=CONN_BRIDGE_SERIAL, hexfile=NRF91_BOOTLOADER)
    except Exception as e:
        logger.error(f"Error flashing bootloader: {e}")
    dfu_device(OOB_NRF91_DFU_ZIP, serial=CONN_BRIDGE_SERIAL)


def test_03_verify_oob_boot(uart):
    """
    Confirm OOB firmware is running before cross-application DFU.
    """
    expected_lines = [
        "Attempting to boot slot 0",
        "*** Booting Hello, nRF Cloud",
    ]
    time.sleep(2)
    uart.write("kernel reboot cold\r\n")
    uart.wait_for_str(expected_lines, timeout=120)


def test_04_dfu_oob_to_att(att_dfu_zip):
    """
    DFU from OOB firmware to latest Asset Tracker Template release over MCUboot.
    """
    logger.info("DFU to Asset Tracker Template bundle: %s", att_dfu_zip)
    dfu_device(att_dfu_zip, serial=CONN_BRIDGE_SERIAL)
    wait_until_uart_available(CONN_BRIDGE_SERIAL)


def test_05_verify_att_boot(uart):
    """
    Confirm Asset Tracker Template firmware boots after DFU.
    """
    expected_lines = [
        "Attempting to boot slot 0",
        "*** Booting Asset Tracker Template",
        "cloud: cloud_module_thread: Cloud module task started",
    ]
    time.sleep(2)
    uart.write("kernel reboot cold\r\n")
    uart.wait_for_str(expected_lines, timeout=120)
