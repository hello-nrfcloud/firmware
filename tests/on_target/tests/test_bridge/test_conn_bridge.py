##########################################################################################
# Copyright (c) 2024 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
##########################################################################################

import pytest
import os
import types
import time
from utils.flash_tools import (
    flash_device,
    recover_device,
    get_first_artifact_match,
    pyocd_flash_device,
)
from utils.uart import Uart, wait_until_uart_available
from tests.conftest import get_uarts
import sys

sys.path.append(os.getcwd())
from utils.logger import get_logger

logger = get_logger()

# All tests in this file are marked slow
pytestmark = pytest.mark.slow

NRF53_NET_HEX_FILE = get_first_artifact_match(
    "artifacts/connectivity-bridge-*-thingy91x-nrf53-net.hex"
)

NRF53_APP_HEX_FILE = get_first_artifact_match(
    "artifacts/connectivity-bridge-*-thingy91x-nrf53-app.hex"
)

BRIDGE_TEST_APP = "artifacts/bridge_test_app.hex"
UART_TIMEOUT = 10
BLE_CONTROL_MESSAGE_RETRIES = 3
SEGGER_NRF53 = os.getenv("SEGGER_NRF53")

CONNECTIVITY_BRIDGE_UART = f"THINGY91X_{os.getenv('UART_ID_DUT_2')}"


@pytest.fixture(scope="module")
def uarts():
    """
    Setup connectivity bridge hardware/firmware
    """
    logger.info("Recovering nRF53 network core")
    recover_device(serial=SEGGER_NRF53, core="Network")
    logger.info("Recovering nRF53 application core")
    recover_device(serial=SEGGER_NRF53, core="Application")
    logger.info("Flashing nRF53 with connectivity bridge firmware")
    logger.info("Flashing nRF53 network core")
    flash_device(
        hexfile=NRF53_NET_HEX_FILE,
        serial=SEGGER_NRF53,
        extra_args=[
            "--core",
            "Network",
            "--options",
            "reset=RESET_NONE,chip_erase_mode=ERASE_ALL,verify=VERIFY_NONE",
        ],
    )
    logger.info("Flashing nRF53 application core")
    flash_device(
        hexfile=NRF53_APP_HEX_FILE,
        serial=SEGGER_NRF53,
        extra_args=[
            "--options",
            "reset=RESET_SYSTEM,chip_erase_mode=ERASE_ALL,verify=VERIFY_NONE",
        ],
    )
    logger.info("Waiting for connectivity bridge UART to be available")
    wait_until_uart_available(CONNECTIVITY_BRIDGE_UART)

    logger.info("nRF53 initialized successfully")
    pyocd_flash_device(serial=CONNECTIVITY_BRIDGE_UART, hexfile=BRIDGE_TEST_APP)

    all_uarts = get_uarts(CONNECTIVITY_BRIDGE_UART)
    if not all_uarts:
        pytest.fail("No UARTs found")
    logger.info(f"All uarts discovered: {all_uarts}")
    uart0_string = all_uarts[0]
    uart1_string = all_uarts[1]
    logger.info(f"UART0: {uart0_string}")
    logger.info(f"UART1: {uart1_string}")

    uart0 = Uart(uart0_string, name="Thingy91_UART0")
    uart1 = Uart(uart1_string, name="Thingy91_UART1", baudrate=1000000)

    yield types.SimpleNamespace(uart0=uart0, uart1=uart1)

    uart0.stop()
    uart1.stop()


def test_01_conn_bridge_uarts(uarts):
    """
    Check that uarts are working
    """
    uarts.uart0.wait_for_str("UART0 running at baudrate 115200", timeout=UART_TIMEOUT)
    uarts.uart1.wait_for_str("UART1 running at baudrate 1000000", timeout=UART_TIMEOUT)


def test_02_conn_bridge_uart_control_msg(uarts):
    """
    Check that control message is received on UART0
    """
    uarts.uart0.write(b"CHECK_UART0_SMOKE\r\n")
    uarts.uart0.wait_for_str(
        "This message should be seen on UART0",
        timeout=UART_TIMEOUT,
    )


def test_03_conn_bridge_4k_data_uart1_usb(uarts):
    """
    Test 4k of data sent over UART1 to USB
    """
    uarts.uart0.flush()
    uarts.uart1.flush()
    ctrl_str = generate_control_str(40)
    uarts.uart0.write(b"CHECK_UART1_4k_TRACES\r\n")
    uarts.uart0.wait_for_str("4k of data sent over UART1", timeout=UART_TIMEOUT)
    uarts.uart1.wait_for_str(ctrl_str, timeout=UART_TIMEOUT)


def test_04_conn_bridge_4k_data_uart0(uarts):
    """
    Test 4k of data sent over UART0 to nRF9160
    """
    uarts.uart0.flush()
    uarts.uart1.flush()
    ctrl_str = generate_control_str_no_newlines(40)
    ctrl_str_in_bytes = ctrl_str.encode("utf-8")
    uarts.uart0.write(ctrl_str_in_bytes)
    # The last control string should contain "0039"
    uarts.uart0.wait_for_str("0039", timeout=UART_TIMEOUT)

    log = uarts.uart0.log
    control_str_count = log.count(control_str_base())
    assert control_str_count == 40, f"Found control strings: {control_str_count}\n{log}"
    assert log.count("Unexpected message received:") == 1, log
    missing_indexes = [x for x in range(40) if f"{x:04d}" not in log]
    assert not missing_indexes, f"{log}\nMissing indexes: {missing_indexes}"


def test_05_conn_bridge_400k_data_uart1_usb(uarts):
    """
    Test 400k of data sent over UART1 to USB
    """
    uarts.uart1.flush()
    uarts.uart0.write(b"CHECK_UART1_400k_TRACES\r\n")
    # The last control string should contain "0399"
    uarts.uart1.wait_for_str("0399", timeout=10)

    log = uarts.uart1.log
    control_str_count = log.count(control_str_base())
    assert control_str_count == 4400, (
        f"{log}\nFound control strings: {control_str_count}"
    )
    missing_indexes = [x for x in range(400) if f"{x:04d}" not in log]
    assert not missing_indexes, f"{log}\nMissing indexes: {missing_indexes}"


def test_06_conn_bridge_random_delay_uart0(uarts):
    """
    Test random delay on UART0
    """
    uarts.uart0.flush()
    uarts.uart0.write(b"CHECK_UART0_RANDOM_DELAY\r\n")
    time.sleep(3)
    assert uarts.uart0.log.count(".") == 2500, uarts.uart0.log
    dotline = "..................................................\n"
    assert uarts.uart0.log.count(dotline) == 49, uarts.uart0.log


def test_07_conn_bridge_random_delay_uart1(uarts):
    """
    Test random delay on UART1
    """
    uarts.uart1.flush()
    uarts.uart0.write(b"CHECK_UART1_RANDOM_DELAY\r\n")
    time.sleep(2)
    print(uarts.uart1.log)
    assert uarts.uart1.log.count(".") == 2500, uarts.uart1.log
    dotline = "..................................................\n"
    assert uarts.uart1.log.count(dotline) == 49, uarts.uart1.log

def control_str_base():
    return "".join(map(chr, range(40, 127)))


def generate_control_str(lines):
    control_str = ""
    for i in range(lines):
        control_str += f"{i:04d}"
        control_str += control_str_base() + "\n"
    return control_str


def generate_control_str_beefy(lines):
    control_str = ""
    for i in range(lines):
        control_str += f"{i:04d}"
        control_str += (control_str_base() + "\n") * 11
    return control_str


def generate_control_str_no_newlines(lines):
    control_str = ""
    for i in range(lines):
        control_str += f"{i:04d}"
        control_str += control_str_base()
    control_str += "\n"
    return control_str
