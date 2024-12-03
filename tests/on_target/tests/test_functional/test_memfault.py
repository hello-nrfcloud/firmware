##########################################################################################
# Copyright (c) 2024 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
##########################################################################################

import pytest
import os
import requests
import datetime
import time
from utils.flash_tools import flash_device, reset_device
from utils.logger import get_logger

MEMFAULT_ORG_TOKEN = os.getenv('MEMFAULT_ORGANIZATION_TOKEN')
MEMFAULT_ORG = os.getenv('MEMFAULT_ORGANIZATION_SLUG')
MEMFAULT_PROJ = os.getenv('MEMFAULT_PROJECT_SLUG')
IMEI = os.getenv('IMEI')
MEMFAULT_TIMEOUT = 5 * 60

logger = get_logger()

url = "https://api.memfault.com/api/v0"
auth = ("", MEMFAULT_ORG_TOKEN)

def get_event_log():
    response = requests.get(f"{url}/organizations/{MEMFAULT_ORG}/projects/{MEMFAULT_PROJ}/event-log", auth=auth)
    response.raise_for_status()
    return response.json()

def get_latest_events(event_type, device_id):
    data = get_event_log()
    latest_events = [
        x
        for x in data["data"]
        if x["device_serial"] == str(device_id) and x["type"] == event_type
    ]
    return latest_events

def timestamp(event):
    return datetime.datetime.strptime(
        event["captured_date"], "%Y-%m-%dT%H:%M:%S.%f%z"
    )

def wait_for_heartbeat(timestamp_old_heartbeat_evt):
    new_heartbeat_found = False
    start = time.time()
    while time.time() - start < MEMFAULT_TIMEOUT:
        logger.debug("Looking for latest heartbeat events")
        time.sleep(5)
        new_heartbeat_events = get_latest_events("HEARTBEAT", IMEI)
        if not new_heartbeat_events:
            continue
        if not new_heartbeat_events[0]["summary"]["event_info"]["metrics"]["MemfaultSdkMetric_UnexpectedRebootCount"] >= 1:
            continue
        # Check that we have heartbeat event with newer timestamp
        if not timestamp_old_heartbeat_evt:
            new_heartbeat_found = True
        elif timestamp(new_heartbeat_events[0]) > timestamp_old_heartbeat_evt and not new_heartbeat_found:
            new_heartbeat_found = True
        if new_heartbeat_found:
            break
    else:
        raise AssertionError("No new heartbeat event observed")

def test_memfault(t91x_board, hex_file):
    flash_device(os.path.abspath(hex_file))
    t91x_board.uart.xfactoryreset()
    patterns_memfault = [
            "Network connectivity established",
            "memfault: memfault_task: Memfault module task started",
            "memfault: memfault_task: Cloud status received",
    ]

    # Save timestamp of latest heartbeat event
    heartbeat_events = get_latest_events("HEARTBEAT", IMEI)
    timestamp_old_heartbeat_evt = timestamp(heartbeat_events[0]) if heartbeat_events else  None

    t91x_board.uart.flush()
    reset_device()
    t91x_board.uart.wait_for_str(patterns_memfault, timeout=60)

    # Trigger bus fault to generate memfault event
    t91x_board.uart.write("mflt test busfault\r\n")

    wait_for_heartbeat(timestamp_old_heartbeat_evt)
