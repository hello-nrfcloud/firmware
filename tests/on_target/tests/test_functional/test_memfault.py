##########################################################################################
# Copyright (c) 2024 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
##########################################################################################

import os
import requests
from datetime import datetime
import time
from utils.flash_tools import flash_device, reset_device
from utils.logger import get_logger

MEMFAULT_ORG_TOKEN = os.getenv('MEMFAULT_ORGANIZATION_TOKEN')
MEMFAULT_ORG = os.getenv('MEMFAULT_ORGANIZATION_SLUG')
MEMFAULT_PROJ = os.getenv('MEMFAULT_PROJECT_SLUG')
IMEI = os.getenv('IMEI')
MEMFAULT_TIMEOUT = 5 * 60

# Memfault chunks are relayed through nRF Cloud CoAP and attributed to the nRF Cloud device ID
DEVICE_SERIALS = (f"oob-{IMEI}", IMEI)

logger = get_logger()

url = "https://api.memfault.com/api/v0"
auth = ("", MEMFAULT_ORG_TOKEN)

def get_traces(family):
    r = requests.get(
        f"{url}/organizations/{MEMFAULT_ORG}/projects/{MEMFAULT_PROJ}/traces",
        auth=auth,
        timeout=10
    )
    r.raise_for_status()
    return [x for x in r.json()["data"] if x["source_type"] == family]

def timestamp(trace):
    return datetime.fromisoformat(trace["captured_date"].replace("Z", "+00:00"))

def get_latest_coredump_traces():
    coredumps = get_traces("coredump")
    device_coredumps = [x for x in coredumps if x["device"]["device_serial"] in DEVICE_SERIALS]
    if not device_coredumps:
        serials = {x["device"]["device_serial"] for x in coredumps}
        logger.debug(f"No coredumps for {DEVICE_SERIALS}, recent coredump serials: {serials}")
    return sorted(device_coredumps, key=timestamp, reverse=True)

def wait_for_coredump(timestamp_old_coredump):
    start = time.time()
    while time.time() - start < MEMFAULT_TIMEOUT:
        time.sleep(5)
        coredumps = get_latest_coredump_traces()
        if not coredumps:
            continue
        logger.debug(f"Latest coredump: {coredumps[0]['device']['device_serial']} "
                     f"captured {coredumps[0]['captured_date']}")
        if not timestamp_old_coredump or timestamp(coredumps[0]) > timestamp_old_coredump:
            return
    raise AssertionError("No new coredump observed")

def test_memfault(t91x_board, hex_file):
    flash_device(os.path.abspath(hex_file))
    t91x_board.uart.xfactoryreset()
    patterns_memfault = [
            "Network connectivity established",
            "memfault: memfault_task: Memfault module task started",
            "memfault: memfault_task: Cloud status received",
    ]

    # Save timestamp of latest coredump
    coredumps = get_latest_coredump_traces()
    timestamp_old_coredump = timestamp(coredumps[0]) if coredumps else None
    logger.debug(f"Timestamp old coredump: {timestamp_old_coredump}")

    t91x_board.uart.flush()
    reset_device()
    t91x_board.uart.wait_for_str(patterns_memfault, timeout=120)

    # Trigger bus fault to generate coredump
    t91x_board.uart.write("mflt test busfault\r\n")

    wait_for_coredump(timestamp_old_coredump)
