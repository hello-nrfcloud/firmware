##########################################################################################
# Copyright (c) 2024 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
##########################################################################################


import time
import os
import sys
import pytest

sys.path.append(os.getcwd())
from utils.logger import get_logger
from utils.flash_tools import (
    flash_device,
    recover_device,
    pyocd_flash_device,
    dfu_device,
    get_first_artifact_match,
)
from utils.uart import Uart, wait_until_uart_available
from tests.conftest import get_uarts

logger = get_logger()

NRF53_NET_HEX_FILE = get_first_artifact_match(
    "artifacts/connectivity-bridge-*-thingy91x-nrf53-net.hex"
)
NRF53_APP_HEX_FILE = get_first_artifact_match(
    "artifacts/connectivity-bridge-*-thingy91x-nrf53-app.hex"
)
NRF53_APP_UPDATE_ZIP = get_first_artifact_match(
    "artifacts/connectivity-bridge-*-thingy91x-nrf53-dfu.zip"
)
NRF53_BL_UPDATE_ZIP = get_first_artifact_match(
    "artifacts/connectivity-bridge-*-thingy91x-nrf53-bootloader.zip"
)
NRF91_APP_UPDATE_ZIP = get_first_artifact_match(
    "artifacts/hello.nrfcloud.com-*-thingy91x-nrf91-dfu.zip"
)
NRF91_BL_UPDATE_ZIP = get_first_artifact_match(
    "artifacts/hello.nrfcloud.com-*-thingy91x-nrf91-bootloader.zip"
)
NRF91_BOOTLOADER = "artifacts/nrf91-bl-v2.hex"

SEGGER_NRF53 = os.getenv("SEGGER_NRF53")
CONN_BRIDGE_SERIAL = f"THINGY91X_{os.getenv('UART_ID_DUT_2')}"


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
    Setup nrf53 using segger probe
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


def test_02_setup_nrf91():
    """
    setup nrf91, flash bootloader only
    """
    wait_until_uart_available(CONN_BRIDGE_SERIAL)
    try:
        pyocd_flash_device(serial=CONN_BRIDGE_SERIAL, hexfile=NRF91_BOOTLOADER)
    except Exception as e:
        logger.error(f"Error flashing bootloader: {e}")


def test_03_dfu_app_nrf91():
    """
    DFU nRF91 APP
    """
    dfu_device(NRF91_APP_UPDATE_ZIP, serial=CONN_BRIDGE_SERIAL)


def test_04_dfu_nrf91_app_output(uart):
    """
    Check app working after nRF91 DFU
    """
    expected_lines = [
        "Attempting to boot slot 0",
        "Firmware version 2",
        "*** Booting Hello, nRF Cloud",
    ]
    time.sleep(2)
    uart.write("kernel reboot cold\r\n")
    uart.wait_for_str(expected_lines, timeout=5)


def test_05_dfu_app_nrf53():
    """
    DFU nRF53 APP
    """
    logger.info("Starting nRF53 APP DFU")
    dfu_device(NRF53_APP_UPDATE_ZIP, serial=CONN_BRIDGE_SERIAL)
    wait_until_uart_available(CONN_BRIDGE_SERIAL)


def test_06_dfu_nrf53_app_output(uart):
    """
    Check app working after nRF53 DFU
    """
    expected_lines = [
        "Attempting to boot slot 0",
        "Firmware version 2",
        "*** Booting Hello, nRF Cloud",
    ]
    time.sleep(2)
    uart.write("kernel reboot cold\r\n")
    uart.wait_for_str(expected_lines, timeout=5)
