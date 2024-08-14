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

@pytest.mark.dut1
def test_program_board_and_check_uart(t91x_board, hex_file):
    flash_device(os.path.abspath(hex_file))
    time.sleep(5)
    t91x_board.uart.xfactoryreset()
    patterns_boot = [
            "Network connectivity established",
            "Connected to Cloud",
            "trigger: trigger_work_fn: Sending data sample trigger",
            "battery: sample: State of charge:",
            "environmental_module: sample: temp:",
            "transport: state_connected_ready_run: Payload",
            "blocked_run: Location search done",
            "trigger: frequent_poll_entry: frequent_poll_entry"
    ]
    patterns_button_press = [
        "trigger: frequent_poll_run: Button 1 pressed in frequent poll state, restarting duration timer",
        "location_module: location_event_handler: Got location: lat:",
        "location: location_utils_event_dispatch: Done",
        "blocked_run: Location search done",
        "trigger: frequent_poll_entry: frequent_poll_entry"
    ]
    patterns_lte_offline = [
            "network: Network connectivity lost",
    ]
    patterns_lte_normal = [
            "network: Network connectivity established",
            "transport: Connected to Cloud"
    ]

    # Boot
    t91x_board.uart.flush()
    reset_device()
    t91x_board.uart.wait_for_str(patterns_boot, timeout=120)

    # Button press
    t91x_board.uart.flush()
    t91x_board.uart.write("zbus button_press\r\n")
    t91x_board.uart.wait_for_str(patterns_button_press, timeout=120)

    # LTE disconnect
    t91x_board.uart.flush()
    t91x_board.uart.write("lte offline\r\n")
    t91x_board.uart.wait_for_str(patterns_lte_offline, timeout=20)

    # LTE reconnect
    t91x_board.uart.flush()
    t91x_board.uart.write("lte normal\r\n")
    t91x_board.uart.wait_for_str(patterns_lte_normal, timeout=120)
