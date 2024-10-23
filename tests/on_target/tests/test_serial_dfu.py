##########################################################################################
# Copyright (c) 2024 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
##########################################################################################

import pytest
import time
import os
import types
from utils.flash_tools import flash_device, reset_device, recover_device, setup_jlink, dfu_device
from utils.uart import Uart
from tests.conftest import get_uarts
import sys
sys.path.append(os.getcwd())
from utils.logger import get_logger

logger = get_logger()

def wait_until_uart_available(name, timeout_seconds=60):
    base_path = "/dev/serial/by-id"
    while timeout_seconds > 0:
        try:
            serial_paths = [os.path.join(base_path, entry) for entry in os.listdir(base_path)]
            for path in sorted(serial_paths):
                if name in path:
                    logger.info(f"UART found: {path}")
                    return path
        except (FileNotFoundError, PermissionError) as e:
            logger.info(f"Uart not available yet (error: {e}), retrying...")
        time.sleep(1)
        timeout_seconds -= 1
    logger.error(f"UART '{name}' not found within {timeout_seconds} seconds")
    return None

@pytest.fixture(scope="function")
def t91x_dfu():
    '''
    This fixture initializes the nRF53 and nRF91 for dfu test.
    First nRF91 is flashed with oob bootloader fw through segger fw on nRF53.
    Then nRF53 is flashed with connectivity bridge fw.
    '''
    SEGGER_NRF53 = os.getenv('SEGGER_NRF53')
    SEGGER_NRF91 = os.getenv('SEGGER_NRF91')
    CONNECTIVITY_BRIDGE_UART = os.getenv('UART_ID')
    NRF53_NET_HEX_FILE = os.getenv('NRF53_NET_HEX_FILE')
    NRF53_APP_HEX_FILE = os.getenv('NRF53_APP_HEX_FILE')
    NRF91_HEX_FILE = os.getenv('NRF91_HEX_FILE')

    logger.info(("Flashing nRF53 with Segger firmware"))
    setup_jlink(SEGGER_NRF53)

    logger.info("Waiting for segger UART to be available")
    wait_until_uart_available(SEGGER_NRF91)

    logger.info("Flashing nRF91 bootloader firmware")
    flash_device(hexfile=NRF91_HEX_FILE, serial=SEGGER_NRF91)
    logger.info("nRF91 initialized successfully")

    logger.info("Recovering nRF53 network core")
    recover_device(serial=SEGGER_NRF53, core='Network')
    logger.info("Recovering nRF53 application core")
    recover_device(serial=SEGGER_NRF53, core='Application')
    logger.info("Flashing nRF53 with connectivity bridge firmware")
    logger.info("Flashing nRF53 network core")
    flash_device(hexfile=NRF53_NET_HEX_FILE, serial=SEGGER_NRF53, extra_args=['--core', 'Network', '--options', 'reset=RESET_NONE,chip_erase_mode=ERASE_ALL,verify=VERIFY_NONE'])
    logger.info("Flashing nRF53 application core")
    flash_device(hexfile=NRF53_APP_HEX_FILE, serial=SEGGER_NRF53, extra_args=['--options', 'reset=RESET_SYSTEM,chip_erase_mode=ERASE_ALL,verify=VERIFY_NONE'])
    logger.info("Waiting for connectivity bridge UART to be available")
    wait_until_uart_available(CONNECTIVITY_BRIDGE_UART)

    logger.info("nRF53 initialized successfully")

    all_uarts = get_uarts()
    if not all_uarts:
        pytest.fail("No UARTs found")
    logger.info(f"All uarts discovered: {all_uarts}")
    log_uart_string = all_uarts[0]
    logger.info(f"Log UART: {log_uart_string}")

    uart = Uart(log_uart_string, timeout=60)

    yield types.SimpleNamespace(uart=uart)

    uart.stop()


@pytest.mark.dut2
@pytest.mark.dfu
def test_dfu(t91x_dfu):
    SEGGER_NRF53 = os.getenv('SEGGER_NRF53')
    CONNECTIVITY_BRIDGE_UART = "THINGY91X_" + os.getenv('UART_ID')

    NRF91_APP_UPDATE_ZIP = os.getenv('NRF91_APP_UPDATE_ZIP')
    NRF91_BL_UPDATE_ZIP = os.getenv('NRF91_BL_UPDATE_ZIP')

    NRF53_APP_UPDATE_ZIP = os.getenv('NRF53_APP_UPDATE_ZIP')
    NRF53_BL_UPDATE_ZIP = os.getenv('NRF53_BL_UPDATE_ZIP')

    logger.info("Starting DFU, stopping UART")
    t91x_dfu.uart.stop()

    # nRF91 APP DFU
    logger.info("Starting nRF91 APP DFU")
    dfu_device(NRF91_APP_UPDATE_ZIP, serial=CONNECTIVITY_BRIDGE_UART)

    # nRF91 BL DFU
    for _ in range(3):
        try:
            logger.info("Starting nRF91 BL DFU")
            dfu_device(NRF91_BL_UPDATE_ZIP, serial=CONNECTIVITY_BRIDGE_UART)
            break
        except Exception as e:
            logger.error(f"An error occurred: {e}, retrying...")
            time.sleep(5)
    else:
        raise Exception("Failed to perform nRF91 BL DFU after 3 attempts.")

    logger.info("nRF91 DFU successful")

    # reset nrf53
    dfu_device(NRF53_APP_UPDATE_ZIP, serial=CONNECTIVITY_BRIDGE_UART, reset_only=True)
    wait_until_uart_available(CONNECTIVITY_BRIDGE_UART)
    time.sleep(20)

    t91x_dfu.uart.start()

    # Look for correct mcuboot firmware version
    expected_lines = ["Firmware version 3", "Zephyr OS"]
    for _ in range(3):
        try:
            # reset nrf91
            dfu_device(NRF91_APP_UPDATE_ZIP, serial=CONNECTIVITY_BRIDGE_UART, reset_only=True)
            t91x_dfu.uart.wait_for_str(expected_lines, timeout=30)
            logger.info(f"{expected_lines} found in logs")
            break
        except Exception:
            logger.error(f"{expected_lines} not found in logs, retrying...")
    else:
        raise Exception(f"{expected_lines} not found in logs")

    t91x_dfu.uart.stop()

    # nRF53 APP DFU
    for _ in range(3):
        try:
            logger.info("Starting nRF53 APP DFU")
            dfu_device(NRF53_APP_UPDATE_ZIP, serial=CONNECTIVITY_BRIDGE_UART)
            wait_until_uart_available(CONNECTIVITY_BRIDGE_UART)
            break
        except Exception as e:
            logger.error(f"An error occurred: {e}, retrying...")
            reset_device(serial=SEGGER_NRF53)
            wait_until_uart_available(CONNECTIVITY_BRIDGE_UART)
            time.sleep(5)
    else:
        raise Exception("Failed to perform nRF53 APP DFU after 3 attempts.")

    # nRF53 BL DFU
    for _ in range(3):
        try:
            logger.info("Starting nRF53 BL DFU")
            dfu_device(NRF53_BL_UPDATE_ZIP, serial=CONNECTIVITY_BRIDGE_UART)
            wait_until_uart_available(CONNECTIVITY_BRIDGE_UART)
            break
        except Exception as e:
            logger.error(f"An error occurred: {e}, retrying...")
            reset_device(serial=SEGGER_NRF53)
            wait_until_uart_available(CONNECTIVITY_BRIDGE_UART)
            time.sleep(5)
    else:
        raise Exception("Failed to perform nRF53 BL DFU after 3 attempts.")

    results = dfu_device(NRF53_BL_UPDATE_ZIP, serial=CONNECTIVITY_BRIDGE_UART, check_53_version=True)
    # assert mcuboot slot 1 has correct version number 2
    assert "S1: 2" in results

    logger.info("nRF53 DFU successful")
