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


def get_uarts():
    base_path = "/dev/serial/by-id"
    try:
        serial_paths = [os.path.join(base_path, entry) for entry in os.listdir(base_path)]
    except (FileNotFoundError, PermissionError) as e:
        logger.error(e)
        return False

    uarts = []

    for path in sorted(serial_paths):
        # Extract device ID or serial number
        if 'SEGGER_J-Link' in path:
            uarts.append(path)
        else:
            continue
    return uarts


@pytest.fixture(scope="module")
def t91x_board():
    all_uarts = get_uarts()
    logger.info(f"All uarts discovered: {all_uarts}")
    log_uart_string = all_uarts[0]
    logger.info(f"Log UART: {log_uart_string}")

    uart = Uart(log_uart_string, timeout=UART_TIMEOUT)

    yield types.SimpleNamespace(
		uart=uart,
		)

    uart.stop()
