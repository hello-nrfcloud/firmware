##########################################################################################
# Copyright (c) 2024 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
##########################################################################################

import time
from unittest.mock import Mock, patch

import pytest
from uart import Uart


def counter():
    i = 0
    while True:
        yield i
        i += 1

@patch("uart.Uart.__init__")
def mocked_uart(uart_init):
    uart_init.return_value = None
    u = Uart("uart")
    u._evt = Mock()
    u._evt.is_set.return_value = False
    return u

@patch("time.time", side_effect=counter())
@patch("time.sleep")
def test_wait_1(time_sleep, time_time):
    u = mocked_uart()
    u.log = "foo123\nbar123\nbaz123\n"
    u.wait_for_str(["foo", "bar", "baz"])

@patch("time.time", side_effect=counter())
@patch("time.sleep")
def test_wait_2_out_of_order(time_sleep, time_time):
    u = mocked_uart()
    u.log = "foo123\nbar123\nbaz123\n"
    u.wait_for_str(["baz", "foo", "bar"], timeout=3)

@patch("time.time", side_effect=counter())
@patch("time.sleep")
def test_wait_3_missing(time_sleep, time_time):
    u = mocked_uart()
    u.log = "foo123\nbar123\nbaz123\n"
    with pytest.raises(AssertionError) as ex_info:
        u.wait_for_str(["baz", "foo", "bar", "1234"], timeout=3)
    assert "1234" in str(ex_info.value)

@patch("time.time", side_effect=counter())
@patch("time.sleep")
def test_wait_ordered_1(time_sleep, time_time):
    u = mocked_uart()
    u.log = "foo123\nbar123\nbaz123\n"
    u.wait_for_str_ordered(["foo", "bar", "baz"], timeout=3)

@patch("time.time", side_effect=counter())
@patch("time.sleep")
def test_wait_ordered_2_missing(time_sleep, time_time):
    u = mocked_uart()
    u.log = "foo123\nbar123\nbaz123\n"
    with pytest.raises(AssertionError) as ex_info:
        u.wait_for_str_ordered(["foo", "bar", "baz", "1234"], timeout=3)
    assert "1234" in str(ex_info.value)

@patch("time.time", side_effect=counter())
@patch("time.sleep")
def test_wait_ordered_2_out_of_order(time_sleep, time_time):
    u = mocked_uart()
    u.log = "foo123\nbar123\nbaz123\n"
    with pytest.raises(AssertionError) as ex_info:
        u.wait_for_str_ordered(["foo", "baz", "bar"], timeout=3)
    assert "bar missing" in str(ex_info.value)
