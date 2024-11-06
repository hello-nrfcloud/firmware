##########################################################################################
# Copyright (c) 2024 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
##########################################################################################

import os
import re
import pytest
import types
from utils.flash_tools import recover_device
from utils.uart import Uart, UartBinary
from utils.hellonrfcloud_fota import HelloNrfCloudFOTA
import sys
sys.path.append(os.getcwd())
from utils.logger import get_logger

logger = get_logger()

UART_TIMEOUT = 60 * 30

T91X_SERIAL_NUMBER = os.getenv('T91X_SERIAL_NUMBER')
T91X_IMEI = os.getenv('T91X_IMEI')
T91X_FINGERPRINT = os.getenv('T91X_FINGERPRINT')

def get_uarts():
    base_path = "/dev/serial/by-id"
    try:
        serial_paths = [os.path.join(base_path, entry) for entry in os.listdir(base_path)]
    except (FileNotFoundError, PermissionError) as e:
        logger.error(e)
        return False

    uarts = []

    for path in sorted(serial_paths):
        if T91X_SERIAL_NUMBER in path:
            uarts.append(path)
        else:
            continue
    return uarts

def scan_log_for_assertions(log):
    assert_counts = log.count("ASSERT")
    if assert_counts > 0:
        pytest.fail(f"{assert_counts} ASSERT found in log: {log}")

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

    serial_number = T91X_SERIAL_NUMBER
    uart = Uart(log_uart_string, timeout=UART_TIMEOUT)
    fota = HelloNrfCloudFOTA(device_id=f"oob-{T91X_IMEI}", \
                                    fingerprint=T91X_FINGERPRINT)

    yield types.SimpleNamespace(
        serial_number=serial_number,
        uart=uart,
        fota=fota
		)

    # Cancel pending fota jobs, at fota test teardown
    if T91X_IMEI:
        try:
            pending_jobs = fota.check_pending_jobs()
            if pending_jobs:
                logger.warning(f"{len(pending_jobs)} pending fota jobs found for fota device")
                logger.info("Canceling pending jobs")
                fota.delete_jobs(pending_jobs)
        except Exception as e:
            logger.error(f"Error during teardown while canceling pending fota jobs: {e}")

    uart_log = uart.whole_log
    uart.stop()
    recover_device()

    scan_log_for_assertions(uart_log)

@pytest.fixture(scope="module")
def t91x_traces(t91x_board):
    all_uarts = get_uarts()
    trace_uart_string = all_uarts[1]
    uart_trace = UartBinary(trace_uart_string)

    yield types.SimpleNamespace(
        trace=uart_trace,
        uart=t91x_board.uart
        )

    uart_trace.stop()

@pytest.fixture(scope="session")
def hex_file():
    # Search for the firmware hex file in the artifacts folder
    artifacts_dir = "artifacts"
    hex_pattern = r"hello\.nrfcloud\.com-[0-9a-z\.]+-thingy91x-nrf91-dfu\.zip"

    for file in os.listdir(artifacts_dir):
        if re.match(hex_pattern, file):
            return os.path.join(artifacts_dir, file)

    pytest.fail("No matching firmware .hex file found in the artifacts directory")
