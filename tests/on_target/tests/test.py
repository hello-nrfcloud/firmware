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

TEST_TIMEOUT = 1 * 60

HEX_FILE = os.path.abspath("tests/night/artifacts/merged.hex")

def test_program_board_and_check_uart(t91x_board):
    flash_device(HEX_FILE)

    expected_lines = ["Network connectivity established", "Connected to Cloud"]

    logger.info("Waiting for expected lines on UART")
    t91x_board.uart.wait_for_str(expected_lines, timeout=TEST_TIMEOUT)
    logger.info("Expected lines found")
