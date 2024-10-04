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
                    break
        except (FileNotFoundError, PermissionError) as e:
            logger.error(e)
        time.sleep(1)
        timeout_seconds -= 1

@pytest.fixture(scope="function")
def t91x_dfu():
    SEGGER_NRF53 = os.getenv('SEGGER_NRF53')
    SEGGER_NRF91 = os.getenv('SEGGER_NRF91')
    CONNECTIVITY_BRIDGE_UART = os.getenv('UART_ID')
    NRF53_HEX_FILE = os.getenv('NRF53_HEX_FILE')
    NRF91_HEX_FILE = os.getenv('NRF91_HEX_FILE')

    setup_jlink(SEGGER_NRF53)
    wait_until_uart_available(SEGGER_NRF91)
    flash_device(hexfile=NRF91_HEX_FILE, serial=SEGGER_NRF91)
    logger.info("nRF91 initialized successfully")
    recover_device(serial=SEGGER_NRF53, core='Network')
    recover_device(serial=SEGGER_NRF53, core='Application')
    flash_device(hexfile=NRF53_HEX_FILE, serial=SEGGER_NRF53, extra_args=['--core', 'Network', '--options', 'reset=RESET_NONE,chip_erase_mode=ERASE_ALL,verify=VERIFY_NONE'])
    flash_device(hexfile=NRF53_HEX_FILE, serial=SEGGER_NRF53, extra_args=['--options', 'reset=RESET_SYSTEM,chip_erase_mode=ERASE_ALL,verify=VERIFY_NONE'])
    wait_until_uart_available(CONNECTIVITY_BRIDGE_UART)

    logger.info("nRF53 initialized successfully")

    all_uarts = get_uarts()
    logger.info(f"All uarts discovered: {all_uarts}")
    if not all_uarts:
        pytest.fail("No UARTs found")
    log_uart_string = all_uarts[0]
    logger.info(f"Log UART: {log_uart_string}")

    uart = Uart(log_uart_string, timeout=60)

    yield types.SimpleNamespace(uart=uart)

    uart.stop()


@pytest.mark.dut2
def test_dfu(t91x_dfu):
    CONNECTIVITY_BRIDGE_UART = "THINGY91X_" + os.getenv('UART_ID')

    NRF91_APP_UPDATE_ZIP = os.getenv('NRF91_APP_UPDATE_ZIP')
    NRF91_BL_UPDATE_ZIP = os.getenv('NRF91_BL_UPDATE_ZIP')

    NRF53_APP_UPDATE_ZIP = os.getenv('NRF53_APP_UPDATE_ZIP')
    NRF53_BL_UPDATE_ZIP = os.getenv('NRF53_BL_UPDATE_ZIP')

    t91x_dfu.uart.stop()

    dfu_device(NRF91_APP_UPDATE_ZIP, serial=CONNECTIVITY_BRIDGE_UART)
    dfu_device(NRF91_BL_UPDATE_ZIP, serial=CONNECTIVITY_BRIDGE_UART)

    t91x_dfu.uart.start()

    expected_lines = ["Firmware version 3", "Zephyr OS"]
    t91x_dfu.uart.wait_for_str(expected_lines, timeout=60)
    logger.info("nRF91 DFU test passed successfully")

    t91x_dfu.uart.stop()

    dfu_device(NRF53_APP_UPDATE_ZIP, serial=CONNECTIVITY_BRIDGE_UART)
    wait_until_uart_available(CONNECTIVITY_BRIDGE_UART)
    # TODO: Fix the CI issue with nRF53 BL DFU
    # dfu_device(NRF53_BL_UPDATE_ZIP, serial=CONNECTIVITY_BRIDGE_UART)
    # wait_until_uart_available(CONNECTIVITY_BRIDGE_UART)

    logger.info("nRF53 DFU successful, checking version")
