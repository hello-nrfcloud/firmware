##########################################################################################
# Copyright (c) 2024 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
##########################################################################################

import os
import re
import types
import pytest
import subprocess
from pytest_metadata.plugin import metadata_key
from utils.flash_tools import recover_device
from utils.uart import Uart, UartBinary
from utils.hellonrfcloud_fota import HelloNrfCloudFOTA
from utils.logger import get_logger

logger = get_logger()

UART_TIMEOUT = 60 * 30

SEGGER = os.getenv('SEGGER')
UART_ID = SEGGER
FOTADEVICE_IMEI = os.getenv('IMEI')
FOTADEVICE_FINGERPRINT = os.getenv('FINGERPRINT')

def pytest_html_report_title(report):
    report.title = os.getenv("TEST_REPORT_NAME", "OOB Test Report")


def check_output(cmd, regexp):
    p = subprocess.check_output(cmd.split()).decode().strip()
    match = re.search(regexp, p)
    return match.group(1)

def pytest_configure(config):
    config.stash[metadata_key]["Board"] = "thingy91x"
    config.stash[metadata_key]["Board revision"] = os.getenv("DUT1_HW_REVISION", "Undefined")
    config.stash[metadata_key]["nrfutil version"] = check_output("nrfutil --version", r"nrfutil ([0-9\.]+)")
    config.stash[metadata_key]["nrfutil-device version"] = check_output(
        "nrfutil device --version",
        r"nrfutil-device ([0-9\.]+)"
    )
    config.stash[metadata_key]["SEGGER JLink version"] = check_output(
        "nrfutil device --version",
        r"SEGGER J-Link version: JLink_V([0-9a-z\.]+)"
    )

# Add column to table for test specification
def pytest_html_results_table_header(cells):
    cells.insert(2, "<th>Specification</th>")

def pytest_html_results_table_row(report, cells):
    cells.insert(2, f"<td>{report.description}</td>")

@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item, call):
    outcome = yield
    report = outcome.get_result()
    report.description = str(item.function.__doc__)

def get_uarts(uart_id):
    base_path = "/dev/serial/by-id"
    try:
        serial_paths = [os.path.join(base_path, entry) for entry in os.listdir(base_path)]
    except (FileNotFoundError, PermissionError) as e:
        logger.error(e)
        return False

    uarts = []

    for path in sorted(serial_paths):
        if uart_id in path:
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

@pytest.fixture(scope="function")
def t91x_board():
    all_uarts = get_uarts(UART_ID)
    if not all_uarts:
        pytest.fail("No UARTs found")
    log_uart_string = all_uarts[0]
    uart = Uart(log_uart_string, timeout=UART_TIMEOUT)

    yield types.SimpleNamespace(
		uart=uart
		)

    uart_log = uart.whole_log
    uart.stop()
    recover_device()

    scan_log_for_assertions(uart_log)

@pytest.fixture(scope="function")
def t91x_fota(t91x_board):
    fota = HelloNrfCloudFOTA(device_id=f"oob-{FOTADEVICE_IMEI}", \
                             fingerprint=FOTADEVICE_FINGERPRINT)

    yield types.SimpleNamespace(
        fota=fota,
        uart=t91x_board.uart
		)

    # Cancel pending fota jobs, at fota test teardown
    if FOTADEVICE_IMEI:
        try:
            pending_jobs = fota.check_pending_jobs()
            if pending_jobs:
                logger.warning(f"{len(pending_jobs)} pending fota jobs found for fota device")
                logger.info("Canceling pending jobs")
                fota.delete_jobs(pending_jobs)
        except Exception as e:
            logger.error(f"Error during teardown while canceling pending fota jobs: {e}")

@pytest.fixture(scope="function")
def t91x_traces(t91x_board):
    all_uarts = get_uarts(UART_ID)
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
    hex_pattern = r"hello\.nrfcloud\.com-[0-9a-z\.]+-thingy91x-nrf91\.hex"

    for file in os.listdir(artifacts_dir):
        if re.match(hex_pattern, file):
            return os.path.join(artifacts_dir, file)

    pytest.fail("No matching firmware .hex file found in the artifacts directory")
