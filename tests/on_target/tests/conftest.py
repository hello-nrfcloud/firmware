##########################################################################################
# Copyright (c) 2024 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
##########################################################################################

import os
import re
import pytest
import types
from utils.uart import Uart
from utils.hellonrfcloud_fota import HelloNrfCloudFOTA
import sys
sys.path.append(os.getcwd())
from utils.logger import get_logger

logger = get_logger()

UART_TIMEOUT = 60 * 15

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


@pytest.hookimpl(tryfirst=True)
def pytest_runtest_logstart(nodeid, location):
    logger.info(f"Starting test: {nodeid}")

@pytest.hookimpl(trylast=True)
def pytest_runtest_logfinish(nodeid, location):
    logger.info(f"Finished test: {nodeid}")

@pytest.fixture(scope="module")
def t91x_board():
    all_uarts = get_uarts()
    if not all_uarts:
        pytest.fail("No UARTs found")
    log_uart_string = all_uarts[0]

    uart = Uart(log_uart_string, timeout=UART_TIMEOUT)

    fota = HelloNrfCloudFOTA()

    yield types.SimpleNamespace(
		uart=uart,
        fota=fota
		)

    uart.stop()

@pytest.fixture(scope="session")
def hex_file():
    # Search for the firmware hex file in the artifacts folder
    artifacts_dir = "artifacts"
    hex_pattern = r"hello\.nrfcloud\.com-[a-f0-9]+-thingy91x-debug-app\.hex"

    for file in os.listdir(artifacts_dir):
        if re.match(hex_pattern, file):
            return os.path.join(artifacts_dir, file)

    pytest.fail("No matching firmware .hex file found in the artifacts directory")
