##########################################################################################
# Copyright (c) 2024 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
##########################################################################################

import os
import re
import pytest
import types
from utils.uart import Uart
import sys
sys.path.append(os.getcwd())
from utils.logger import get_logger

logger = get_logger()

UART_TIMEOUT = 60 * 10

SEGGER = os.getenv('SEGGER')
UART_ID = os.getenv('UART_ID', SEGGER)

def get_uarts():
    base_path = "/dev/serial/by-id"
    try:
        serial_paths = [os.path.join(base_path, entry) for entry in os.listdir(base_path)]
    except (FileNotFoundError, PermissionError) as e:
        logger.error(e)
        return False

    uarts = []

    for path in sorted(serial_paths):
        if 'SEGGER_J-Link' and UART_ID in path:
            uarts.append(path)
        if 'Nordic_Semiconductor_Thingy' and UART_ID in path:
            uarts.append(path)
        else:
            continue
    return uarts


@pytest.fixture(scope="module")
def t91x_board():
    all_uarts = get_uarts()
    if not all_uarts:
        pytest.fail("No UARTs found")
    log_uart_string = all_uarts[0]

    uart = Uart(log_uart_string, timeout=UART_TIMEOUT)

    yield types.SimpleNamespace(
		uart=uart,
		)

    uart.stop()


@pytest.fixture(scope="module")
def hex_file(request):
    return request.config.getoption("--firmware-hex")


# Add support for input arguments
def pytest_addoption(parser):
    parser.addoption(
        "--firmware-hex",
        action="store",
        default="artifacts/merged.hex",
        help="Path to the firmware hex file",
    )
