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

@pytest.mark.wifi
@pytest.mark.dut1
def test_wifi_location(t91x_board, hex_file):
    run_location(t91x_board, hex_file, location_method="Wi-Fi")

@pytest.mark.gnss
def test_gnss_location(t91x_board, hex_file):
    run_location(t91x_board, hex_file, location_method="GNSS")

def run_location(t91x_board, hex_file, location_method):
    flash_device(os.path.abspath(hex_file))
    t91x_board.uart.xfactoryreset()
    patterns_cloud_connection = [
            "Network connectivity established",
            "Connected to Cloud"
    ]

    patterns_location = ["Wi-Fi and cellular methods combined"] if location_method == "Wi-Fi" else []
    patterns_location = patterns_location + [
            "location_event_handler: Got location: lat:",
            "Location search done"]

    # Cloud connection
    t91x_board.uart.flush()
    reset_device()
    t91x_board.uart.wait_for_str(patterns_cloud_connection, timeout=60)

    # Location
    t91x_board.uart.wait_for_str(patterns_location, timeout=180)

    # Extract coordinates from UART output
    location_pattern = re.compile( \
        r"location_event_handler: Got location: lat: ([\d.]+), lon: ([\d.]+), acc: ([\d.]+), method: ([\d.]+)")
    match = location_pattern.search(t91x_board.uart.log)
    assert match, "Failed to extract coordinates from UART output"
    lat, lon, acc, method = match.groups()
    assert abs(float(lat) - 61.5) < 2 and abs(float(lon) - 10.5) < 1, \
        f"Coordinates ({lat}, {lon}) are not in the same part of the world as the test servers."
    method = int(method)
    expected_method = 4 if location_method == "Wi-Fi" else 1
    assert method == expected_method, f"Unexpected location method used {method}, expected: {expected_method}"
