##########################################################################################
# Copyright (c) 2024 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
##########################################################################################

import os
import time
import json
import types
import pytest
import csv
import pandas as pd
import plotly.express as px
from tests.conftest import get_uarts
from ppk2_api.ppk2_api import PPK2_API
from utils.uart import Uart
from utils.flash_tools import flash_device
import sys

sys.path.append(os.getcwd())
from utils.logger import get_logger

logger = get_logger()

UART_TIMEOUT = 60 * 30
POWER_TIMEOUT = 60 * 15
MAX_CURRENT_PSM_UA = 10
SAMPLING_INTERVAL = 0.005
CSV_FILE = "power_measurements.csv"
HMTL_PLOT_FILE = "power_measurements_plot.html"
UART_ID = os.getenv("UART_ID_PPK")


def save_badge_data(average):
    badge_filename = "power_badge.json"
    logger.info(f"Minimum average current measured: {average}uA")
    if average < 0:
        pytest.skip(f"current cant be negative, current average: {average}")
    elif average <= 10:
        color = "green"
    elif average <= 50:
        color = "yellow"
    else:
        color = "red"

    badge_data = {
        "label": "🔗 PSM current uA",
        "message": f"{average}",
        "schemaVersion": 1,
        "color": f"{color}",
    }

    # Save the JSON data to a file
    with open(badge_filename, "w") as json_file:
        json.dump(badge_data, json_file)

    logger.info(f"Minimum average current saved to {badge_filename}")


def save_measurement_data(samples):
    # Generate timestamps for each sample assuming uniform sampling interval
    timestamps = [round(i * SAMPLING_INTERVAL, 2) for i in range(len(samples))]

    with open(CSV_FILE, "w", newline="") as csvfile:
        fieldnames = ["Time (s)", "Current (uA)"]
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)

        writer.writeheader()
        for t, current in zip(timestamps, samples):
            writer.writerow({"Time (s)": t, "Current (uA)": current})

    logger.info(f"Measurement data saved to {CSV_FILE}")


def generate_time_series_html(
    csv_file, date_column, value_column, output_file="time_series_plot.html"
):
    """
    Generates an HTML file with an interactive time series plot from a CSV file.

    Parameters:
    - csv_file (str): Path to the CSV file containing the time series data.
    - date_column (str): Name of the column containing date or time information.
    - value_column (str): Name of the column containing the values to plot.
    - output_file (str): Name of the output HTML file (default is "time_series_plot.html").

    Returns:
    - str: The path to the generated HTML file.
    """
    # Load the CSV file
    df = pd.read_csv(csv_file)

    title = "OOB Current Consumption Plot\n\n"
    note_text = "Note: Low power state is reached after ~10 min, earlier than that uart is active (drawing ≥500uA)"
    title += f"<br><span style='font-size:12px;color:gray;'>{note_text}</span>"

    # Create an interactive Plotly line chart
    fig = px.line(df, x=date_column, y=value_column, title=title)

    # Save the plot to an HTML file
    fig.write_html(output_file)

    logger.info(f"HTML file generated: {output_file}")
    return output_file


def check_shell_operational(s):
    """
    Check if the shell is operational by writing "device" and waiting for "devices"
    """
    for _ in range(5):
        try:
            s.write("device\r\n")
            s.wait_for_str("devices", timeout=2)
            break
        except Exception:
            continue
    else:
        pytest.skip("PPK device is not responding, skipping test")


def get_ppk2_serials():
    """
    Get the serial ports of the PPK2 devices
    """
    ppk2s_connected = PPK2_API.list_devices()
    ppk2s_connected.sort()
    if len(ppk2s_connected) == 2:
        ppk2_port = ppk2s_connected[0]
        ppk2_serial = ppk2s_connected[1]
        logger.info(f"Found PPK2 at port: {ppk2_port}, serial: {ppk2_serial}")
        return ppk2_port, ppk2_serial
    elif len(ppk2s_connected) == 0:
        pytest.skip("No ppk found")
    else:
        pytest.skip(f"PPK should list 2 ports, but found {ppk2s_connected}")


@pytest.fixture(scope="module")
def dut(app_dfu_file):
    """
    This fixture sets up ppk measurement tool.
    """

    ppk2_port, ppk2_serial = get_ppk2_serials()
    shell = Uart(ppk2_serial)
    try:
        check_shell_operational(shell)
        try:
            shell.write("kernel reboot cold\r\n")
            time.sleep(1)
            shell.stop()
        except Exception:
            pass
        time.sleep(1)
        ppk2_port, ppk2_serial = get_ppk2_serials()
        shell = Uart(ppk2_serial)
        check_shell_operational(shell)
        time.sleep(1)
        shell.write("kernel uptime\r\n")
        uptime = shell.wait_for_str_re("Uptime: (.*) ms", timeout=2)
        device_uptime_ms = int(uptime[0])
        if device_uptime_ms > 20000:
            pytest.skip("PPK device was not rebooted, skipping test")
    except Exception as e:
        logger.error(f"Exception when rebooting PPK device: {e}")
    finally:
        shell.stop()

    ppk2_dev = PPK2_API(ppk2_port, timeout=1, write_timeout=1, exclusive=True)

    # get modifier might fail, retry 15 times
    for _ in range(15):
        try:
            ppk2_dev.get_modifiers()
            break
        except Exception as e:
            logger.error(f"Failed to get modifiers: {e}")
            time.sleep(5)
    else:
        pytest.skip("Failed to get ppk modifiers after 10 attempts")

    ppk2_dev.set_source_voltage(3300)
    ppk2_dev.use_ampere_meter()  # set ampere meter mode
    ppk2_dev.toggle_DUT_power("ON")  # enable DUT power

    time.sleep(1)
    for i in range(10):
        try:
            all_uarts = get_uarts(UART_ID)
            if not all_uarts:
                logger.error("No UARTs found")
            log_uart_string = all_uarts[0]
            break
        except Exception as e:
            logger.error(f"Exception when getting uart: {e}")
            time.sleep(1)
            continue
    else:
        pytest.skip("NO uart seen after 10 seconds")

    flash_device(app_dfu_file, UART_ID)
    time.sleep(1)
    t91x_uart = Uart(log_uart_string, timeout=UART_TIMEOUT)

    yield types.SimpleNamespace(ppk2=ppk2_dev, uart=t91x_uart)

    t91x_uart.stop()
    ppk2_dev.stop_measuring()
    ppk2_dev.toggle_DUT_power("OFF")


@pytest.mark.slow
def test_power(dut):
    """
    Test that the device can reach PSM and measure the current consumption

    Current consumption is measured and report generated.
    """
    for _ in range(5):
        try:
            dut.uart.wait_for_str("Connected to Cloud", timeout=30)
            break
        except AssertionError:
            logger.error("Device unable to connect to cloud, reset and retry")
            dut.ppk2.toggle_DUT_power("OFF")
            time.sleep(1)
            dut.ppk2.toggle_DUT_power("ON")
            time.sleep(1)
            continue
    else:
        pytest.skip("Device unable to connect to cloud, skipping test")

    # Disable uart for {POWER_TIMEOUT} seconds
    dut.uart.write(f"uart disable {POWER_TIMEOUT}\r\n")
    dut.ppk2.start_measuring()

    start = time.time()
    last_log_time = start
    psm_reached = False
    interval_sleeper = IntervalSleeper(interval=SAMPLING_INTERVAL)
    rolling_average = RollingAverage(window_size=3)
    while time.time() < start + POWER_TIMEOUT:
        try:
            interval_sleeper.start()
            read_data = dut.ppk2.get_data()
            if read_data == b"":
                interval_sleeper.sleep()
                continue
            ppk_samples, _ = dut.ppk2.get_samples(read_data)
            rolling_average.add_samples(ppk_samples)
            if time.time() - last_log_time >= 3:
                rolling_average_value = rolling_average.get_rolling_average()
                if not rolling_average_value:
                    interval_sleeper.sleep()
                    continue
                logger.info(f"3 sec rolling avg: {rolling_average_value} uA")
                last_log_time = time.time()
                if (
                    rolling_average_value < MAX_CURRENT_PSM_UA
                    and rolling_average_value > 0
                ):
                    psm_reached = True
            interval_sleeper.sleep()
        except Exception as e:
            logger.error(f"Catching exception: {e}")
            pytest.skip("Something went wrong, unable to perform power measurements")
    # Save measurement data and generate HTML report
    save_badge_data(rolling_average.min_rolling_average)
    save_measurement_data(rolling_average.samples)
    generate_time_series_html(CSV_FILE, "Time (s)", "Current (uA)", HMTL_PLOT_FILE)

    # Determine test result based on whether PSM was reached
    if not psm_reached:
        pytest.fail(
            f"PSM target not reached after {POWER_TIMEOUT / 60} minutes, only reached {rolling_average.min_rolling_average} uA"
        )


class RollingAverage:
    def __init__(self, window_size):
        self.window_size = int(window_size / SAMPLING_INTERVAL)
        self.samples = []
        self.series = pd.Series(dtype="float64")
        self.min_rolling_average = float("inf")

    def add_samples(self, samples):
        sample = sum(samples) / len(samples)
        self.samples.append(sample)
        self.series = pd.concat([self.series, pd.Series([sample])], ignore_index=True)

    def get_rolling_average(self):
        rolling_avg_series = self.series.rolling(window=self.window_size).mean()
        rolling_average = rolling_avg_series.iloc[-1]
        rolling_average = (
            round(rolling_average, 2)
            if not pd.isna(rolling_average)
            else rolling_average
        )
        if rolling_average < self.min_rolling_average:
            self.min_rolling_average = rolling_average
        return rolling_average


class IntervalSleeper:
    def __init__(self, interval):
        self.interval = interval
        self.last_time = 0

    def start(self):
        self.last_time = time.perf_counter()

    def sleep(self):
        time_to_sleep = self.interval - (time.perf_counter() - self.last_time)
        if time_to_sleep > 0:
            time.sleep(time_to_sleep)
