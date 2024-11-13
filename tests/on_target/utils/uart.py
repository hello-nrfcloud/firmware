##########################################################################################
# Copyright (c) 2024 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
##########################################################################################

import threading
import serial
import time
import queue
import os
import sys
sys.path.append(os.getcwd())
from utils.logger import get_logger
from typing import Union

DEFAULT_UART_TIMEOUT = 60 * 15
DEFAULT_WAIT_FOR_STR_TIMEOUT = 60 * 5

logger = get_logger()

class UartLogTimeout(Exception):
    pass


class Uart:
    def __init__(
        self,
        uart: str,
        timeout: int = DEFAULT_UART_TIMEOUT,
        baudrate: int = 115200,
        name: str = "",
        serial_timeout: int = 1,
    ) -> None:
        self.baudrate = baudrate
        self.uart = uart
        self.name = name
        self.serial_timeout = serial_timeout
        self.log = ""
        self.whole_log = ""
        self._evt = threading.Event()
        self._writeq = queue.Queue()
        self._t = threading.Thread(target=self._uart)
        self._t.start()
        self._selfdestruct = threading.Timer(
            timeout , self.selfdestruct
        )
        self._selfdestruct.start()

    def write(self, data: bytes) -> None:
        chunked = False
        self._writeq.put((data, chunked))

    def write_chunked(self, data: bytes) -> None:
        chunked = True
        self._writeq.put((data, chunked))

    def at_cmd_write(self, cmd: str) -> None:
        start = time.time()
        log_index = len(self.log)
        count = 0
        while not self._evt.is_set():
            if count % 10 == 0:
                self.write(cmd.encode("utf-8") + b"\r\n")
                log_index = len(self.log)
            count += 1
            time.sleep(0.2)
            if "OK" in self.log[log_index:]:
                break
            if start + 10 < time.time():
                raise UartLogTimeout(f"AT command \"{cmd}\" timed out")

    def xfactoryreset(self) -> None:
        try:
            self.at_cmd_write("at AT")
            self.at_cmd_write("at AT+CFUN=4")
            self.at_cmd_write("at AT%XFACTORYRESET=0")
        except UartLogTimeout:
            logger.error("AT FACTORYRESET failed, continuing")

    def _uart(self) -> None:
        data = None
        s = serial.Serial(
            self.uart, baudrate=self.baudrate, timeout=self.serial_timeout
        )

        if s.in_waiting:
            logger.warning(f"Uart {self.uart} has {s.in_waiting} bytes of unread data, resetting input buffer")
            s.reset_input_buffer()

        if s.out_waiting:
            logger.warning(f"Uart {self.uart} has {s.out_waiting} bytes of unwritten data, resetting output buffer")
            s.reset_output_buffer()

        line = ""
        while not self._evt.is_set():
            if not self._writeq.empty():
                try:
                    write_data, chunked = self._writeq.get_nowait()
                    if isinstance(write_data, str):
                        write_data = write_data.encode('utf-8')
                    if chunked:
                        # Write in chunks to avoid buffer overflows
                        chunk_size = 16
                        chunks = [write_data[i:i + chunk_size] for i in range(0, len(write_data), chunk_size)]
                        for chunk in chunks:
                            s.write(chunk)
                            time.sleep(0.1)
                    else:
                        s.write(write_data)
                    logger.debug(f"UART write {self.name}: {write_data}")
                except queue.Empty:
                    pass

            try:
                read_byte = s.read(1)
                data = read_byte.decode("utf-8")
            except UnicodeDecodeError:
                logger.debug(f"{self.name}: Got unexpected UART value")
                logger.debug(f"{self.name}: Not decodeable data: {hex(ord(read_byte))}")
                continue
            except serial.serialutil.SerialException:
                logger.error(f"{self.name}: Caught SerialException, restarting")
                s.close()
                while True:
                    if self._evt.is_set():
                        return
                    time.sleep(5)
                    try:
                        s = serial.Serial(
                            self.uart,
                            baudrate=self.baudrate,
                            timeout=self.serial_timeout,
                        )
                    except FileNotFoundError:
                        logger.warning(f"{self.uart} not available, retrying")
                        continue
                    break
                continue

            if not data:
                continue

            line = line + data
            if data != "\n":
                continue
            # Full line received
            line = line.strip()
            logger.debug(f"{self.name} {repr(line)}")
            self.log = self.log + "\n" + line
            self.whole_log = self.whole_log + "\n" + line
            line = ""
        s.close()

    def flush(self) -> None:
        self.log = ""

    def selfdestruct(self):
        logger.critical(f"Uart SELFDESTRUCTED {self.name} ({self.uart})")
        self.stop()

    def stop(self) -> None:
        self._selfdestruct.cancel()
        self._evt.set()
        self._t.join()

    def start(self, timeout: int = DEFAULT_UART_TIMEOUT) -> None:
        # Start the UART thread after it has been stopped
        self._evt = threading.Event()
        self._writeq = queue.Queue()
        self._t = threading.Thread(target=self._uart)
        self._t.start()
        self._selfdestruct = threading.Timer(timeout , self.selfdestruct)
        self._selfdestruct.start()

    def wait_for_str_ordered(
        self, msgs: list, error_msg: str = "", timeout: int = DEFAULT_WAIT_FOR_STR_TIMEOUT
    ) -> None:
        start = time.time()
        while True:
            time.sleep(1)
            missing = None
            pos = 0
            for msg in msgs:
                try:
                    pos = self.log.index(msg, pos)
                except ValueError:
                    missing = msg
                    break
                pos += 1
            else:
                break
            if start + timeout < time.time():
                raise AssertionError(
                    f"{missing if missing else msgs} missing in UART log in the expected order. {error_msg}\nUart log:\n{self.log}"
                )
            if self._evt.is_set():
                raise RuntimeError(f"Uart thread stopped, log:\n{self.log}")

    def wait_for_str(self, msgs: Union[str, list], error_msg: str = "", timeout: int = DEFAULT_WAIT_FOR_STR_TIMEOUT) -> None:
        start = time.time()
        msgs = msgs if isinstance(msgs, (list, tuple)) else [msgs]

        while True:
            time.sleep(1)
            missing_msgs = [x for x in msgs if x not in self.log]
            if missing_msgs == []:
                break
            if start + timeout < time.time():
                raise AssertionError(f"{missing_msgs} missing in UART log. {error_msg}\nUart log:\n{self.log}")
            if self._evt.is_set():
                raise RuntimeError(f"Uart thread stopped, log:\n{self.log}")

class UartBinary(Uart):
    def __init__(
        self,
        uart: str,
        timeout: int = DEFAULT_UART_TIMEOUT,
        serial_timeout: int = 5,
        baudrate: int = 1000000,
    ) -> None:
        self.data = b""
        super().__init__(
            uart=uart,
            timeout=timeout,
            baudrate=baudrate,
            serial_timeout=serial_timeout,
        )

    def _uart(self) -> None:
        s = serial.Serial(
            self.uart, baudrate=self.baudrate, timeout=self.serial_timeout
        )
        if s.in_waiting:
            logger.warning(f"Uart {self.uart} has {s.in_waiting} bytes of unread data, resetting input buffer")
            s.reset_input_buffer()

        if s.out_waiting:
            logger.warning(f"Uart {self.uart} has {s.out_waiting} bytes of unwritten data, resetting output buffer")
            s.reset_output_buffer()

        while not self._evt.is_set():
            try:
                data = s.read(8192)
            except serial.serialutil.SerialException:
                logger.error("Caught SerialException, restarting")
                s.close()
                time.sleep(1)
                s = serial.Serial(
                    self.uart, baudrate=self.baudrate, timeout=self.serial_timeout
                )
                continue
            if not data:
                continue
            self.data = self.data + data
        s.close()

    def flush(self) -> None:
        self.data = b""

    def save_to_file(self, filename: str) -> None:
        if len(self.data) == 0:
            logger.warning("No trace data to save")
            return
        with open(filename, "wb" ) as f:
            f.write(self.data)
