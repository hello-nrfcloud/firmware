##########################################################################################
# Copyright (c) 2024 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
##########################################################################################

import pytest
import time
import os
import re
from utils.flash_tools import flash_device, reset_device
import sys
sys.path.append(os.getcwd())
from utils.logger import get_logger

logger = get_logger()

TEST_TIMEOUT = 1 * 60


def check_uart_data(uart, expected_patterns):
    start = time.time()
    while True:
        all_patterns_found = True
        missing_pattern = None
        for pattern in expected_patterns:
            if not pattern.search(uart.log):
                all_patterns_found = False
                missing_pattern = pattern.pattern
                break

        if all_patterns_found or (time.time() > start + TEST_TIMEOUT):
            break

        time.sleep(5)

    assert all_patterns_found, f"Timeout Expired, missing log line pattern: {missing_pattern}"

@pytest.mark.dut1
def test_program_board_and_check_uart(t91x_board, hex_file):
    t91x_board.uart.flush()
    flash_device(os.path.abspath(hex_file))
    time.sleep(5)
    t91x_board.uart.xfactoryreset()
    reset_device()
    expected_lines = ["Network connectivity established",
                        "Connected to Cloud",
                        "trigger: trigger_work_fn: Sending data sample trigger"
                    ]
    t91x_board.uart.wait_for_str(expected_lines, timeout=TEST_TIMEOUT)

    patterns_boot = [
            re.compile(r"battery: sample: State of charge: ([\d.]+)"),
            re.compile(r"environmental_module: sample: temp: ([\d.]+); press: ([\d.]+); humidity: ([\d.]+); iaq: ([\d.]+); CO2: ([\d.]+); VOC: ([\d.]+)"),
            re.compile(r"transport: state_connected_ready_run: Payload\s+([0-9a-fA-F ]+)")
        ]
    check_uart_data(t91x_board.uart, patterns_boot)


    t91x_board.uart.flush()
    # Sleep for 5 seconds to allow any location update to complete before button press
    time.sleep(5)
    # Simulate button press
    t91x_board.uart.write("zbus button_press\r\n")

    patterns_button_press = patterns_boot.copy()
    patterns_button_press.insert(0, re.compile(r"trigger: frequent_poll_run: Button 1 pressed in frequent poll state, restarting duration timer"))
    patterns_button_press.append(re.compile(r"location_module: location_event_handler: Got location: lat: ([\d.]+), lon: ([\d.]+), acc: ([\d.]+)"))

    check_uart_data(t91x_board.uart, patterns_button_press)
