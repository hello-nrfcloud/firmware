##########################################################################################
# Copyright (c) 2024 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
##########################################################################################

import pytest
import time
import os
import types
from utils.flash_tools import flash_device, reset_device, recover_device, setup_jlink, dfu_device
from utils.uart import Uart
from tests.conftest import get_uarts
import sys
sys.path.append(os.getcwd())
from utils.logger import get_logger

UART_TIMEOUT = 60
CB_TIMEOUT = 60
LINE_LEN = 64
LINE_COUNT = 64
BLE_CONTROL_MESSAGE_RETRIES = 3
MAC_ADDR_FLASH_LOCATION = 0x7A000

logger = get_logger()

def wait_until_uart_available(name, timeout_seconds=60):
    base_path = "/dev/serial/by-id"
    while timeout_seconds > 0:
        try:
            serial_paths = [os.path.join(base_path, entry) for entry in os.listdir(base_path)]
            for path in sorted(serial_paths):
                if name in path:
                    logger.info(f"UART found: {path}")
                    return path
        except (FileNotFoundError, PermissionError) as e:
            logger.info(f"Uart not available yet (error: {e}), retrying...")
        time.sleep(1)
        timeout_seconds -= 1
    logger.error(f"UART '{name}' not found within {timeout_seconds} seconds")
    return None

@pytest.fixture(scope="function")
def t91x_conn_bridge():
    '''
    This fixture initializes the nRF53 and nRF91 for connectivity bridge test.
    First the nRF53 is flashed with connectivity bridge fw.
    Then dfu is performed on nrf91 with custom test app.
    '''
    SEGGER_NRF53 = os.getenv('SEGGER_NRF53')
    CONNECTIVITY_BRIDGE_UART = "THINGY91X_" + os.getenv('UART_ID')
    NRF53_NET_HEX_FILE = os.getenv('NRF53_NET_HEX_FILE')
    NRF53_APP_HEX_FILE = os.getenv('NRF53_APP_HEX_FILE')
    NRF91_CUSTOM_APP_ZIP = os.getenv('NRF91_CUSTOM_APP_ZIP')

    logger.info("Recovering nRF53 network core")
    recover_device(serial=SEGGER_NRF53, core='Network')
    logger.info("Recovering nRF53 application core")
    recover_device(serial=SEGGER_NRF53, core='Application')
    logger.info("Flashing nRF53 with connectivity bridge firmware")
    logger.info("Flashing nRF53 network core")
    flash_device(hexfile=NRF53_NET_HEX_FILE, serial=SEGGER_NRF53, extra_args=['--core', 'Network', '--options', 'reset=RESET_NONE,chip_erase_mode=ERASE_ALL,verify=VERIFY_NONE'])
    logger.info("Flashing nRF53 application core")
    flash_device(hexfile=NRF53_APP_HEX_FILE, serial=SEGGER_NRF53, extra_args=['--options', 'reset=RESET_SYSTEM,chip_erase_mode=ERASE_ALL,verify=VERIFY_NONE'])
    logger.info("Waiting for connectivity bridge UART to be available")
    wait_until_uart_available(CONNECTIVITY_BRIDGE_UART)

    logger.info("nRF53 initialized successfully")

    logger.info("Flashing nrf91 with custom app through dfu")
    dfu_device(NRF91_CUSTOM_APP_ZIP, serial=CONNECTIVITY_BRIDGE_UART)

    all_uarts = get_uarts()
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


@pytest.mark.dut2
@pytest.mark.conn_bridge
def test_dfu(t91x_conn_bridge):
    t91x_conn_bridge.uart0.wait_for_str(
            "UART0 running at baudrate 115200", timeout=UART_TIMEOUT
        )
    t91x_conn_bridge.uart1.wait_for_str(
            "UART1 running at baudrate 1000000", timeout=UART_TIMEOUT
        )
    for i in range(BLE_CONTROL_MESSAGE_RETRIES):
        logger.debug(f"Requesting BLE control message try: {i + 1}")
        t91x_conn_bridge.uart0.write(b"CHECK_BLE\r\n")
        try:
            t91x_conn_bridge.uart0.wait_for_str(
                "This message should be seen on UART0 and BLE!",
                timeout=UART_TIMEOUT,
            )
            break
        except AssertionError:
            continue
    else:
        raise AssertionError(
            f"Control message not seen on UART1 after {BLE_CONTROL_MESSAGE_RETRIES} retries"
        )

    # Test large data tx on uart1 from nrf9160 to usb
    ctrl_str = generate_control_str()
    t91x_conn_bridge.uart1.flush()
    t91x_conn_bridge.uart0.write(b"CHECK_UART1\r\n")
    try:
        t91x_conn_bridge.uart1.wait_for_str(ctrl_str, timeout=UART_TIMEOUT)
    except Exception as e:
        logger.error(f"wait_for_str failed, exception: {e}")
        log = t91x_conn_bridge.uart1.log.split()
        ctrl_str = ctrl_str.split()
        count = 0
        for count, log_line in enumerate(log):
            if ctrl_str[0] in log_line:
                break
        else:
            raise AssertionError(
                "first control line not found in log:\n" + ctrl_str[0]
            ) from AssertionError
        for line in ctrl_str:
            assert line in log[count], line + " not in " + log[count]
            count = count + 1

    # Test large data tx on uart0 from usb to nrf9160
    t91x_conn_bridge.uart0.flush()
    ctrl_str = ctrl_str + '\n'
    ctrl_str_in_bytes = ctrl_str.encode("utf-8")

    logger.debug(f"Sending {len(ctrl_str_in_bytes)} bytes of data from usb over UART0")
    t91x_conn_bridge.uart0.write(ctrl_str_in_bytes)

    t91x_conn_bridge.uart0.wait_for_str('Control string received from usb via bridge to nrf9160!', timeout=10)
    logger.debug(f"{len(ctrl_str_in_bytes)} bytes of data sent from usb and received to nrf9160")

def generate_control_str():
    control_str = ""
    for i in range(LINE_COUNT):
        for j in range(i, i + LINE_LEN - 1):
            c = chr(33 + j % LINE_LEN)
            c = "a" if c in ['"', "'", "\\"] else c
            control_str = control_str + c
        control_str = control_str + 'A'
    return control_str[:-1]
