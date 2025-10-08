##########################################################################################
# Copyright (c) 2024 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
##########################################################################################

import subprocess
import os
import sys
import glob

sys.path.append(os.getcwd())
from utils.logger import get_logger
from utils.thingy91x_dfu import detect_family_from_zip, Thingy91XDFU
from pyocd.core.helpers import ConnectHelper
from pyocd.flash.file_programmer import FileProgrammer

logger = get_logger()

SEGGER = os.getenv("SEGGER")


def reset_device(serial=SEGGER, reset_kind="RESET_SYSTEM"):
    logger.info(f"Resetting device, segger: {serial}")
    try:
        result = subprocess.run(
            [
                "nrfutil",
                "device",
                "reset",
                "--serial-number",
                serial,
                "--reset-kind",
                reset_kind,
            ],
            check=True,
            text=True,
            capture_output=True,
        )
    except subprocess.CalledProcessError as e:
        # Handle errors in the command execution
        logger.info("An error occurred while resetting the device.")
        logger.info("Error output:")
        logger.info(e.stderr)
        raise


def flash_device(hexfile, serial=SEGGER, extra_args=[]):
    # hexfile (str): Full path to file (hex or zip) to be programmed
    if not isinstance(hexfile, str):
        raise ValueError("hexfile cannot be None")
    logger.info(f"Flashing device, segger: {serial}, firmware: {hexfile}")
    try:
        result = subprocess.run(
            [
                "nrfutil",
                "device",
                "program",
                *extra_args,
                "--firmware",
                hexfile,
                "--serial-number",
                serial,
            ],
            check=True,
            text=True,
            capture_output=True,
        )
    except subprocess.CalledProcessError as e:
        # Handle errors in the command execution
        logger.info("An error occurred while flashing the device.")
        logger.info("Error output:")
        logger.info(e.stderr)
        raise

    reset_device(serial)


def recover_device(serial=SEGGER, core="Application"):
    logger.info(f"Recovering device, segger: {serial}")
    try:
        result = subprocess.run(
            ["nrfutil", "device", "recover", "--serial-number", serial, "--core", core],
            check=True,
            text=True,
            capture_output=True,
        )
    except subprocess.CalledProcessError as e:
        # Handle errors in the command execution
        logger.info("An error occurred while recovering the device.")
        logger.info("Error output:")
        logger.info(e.stderr)
        raise

def enter_bootloader_mode(serial, chip):
    t91x_dfu = Thingy91XDFU(serial_number=serial, chip=chip)
    t91x_dfu.enter_bootloader_mode()

def dfu_reset_device(serial, chip):
    t91x_dfu = Thingy91XDFU(serial_number=serial, chip=chip)
    t91x_dfu.reset_device()


def dfu_device(
    zipfile, serial=None, reset_only=False, check_53_version=False, bootloader_slot=1
):
    chip, is_mcuboot = detect_family_from_zip(zipfile)
    if chip is None:
        logger.error("Could not determine chip family from image")
        raise ValueError("Invalid image file")
    command = [
        "python3",
        "utils/thingy91x_dfu.py",
        "--image",
        zipfile,
        "--chip",
        chip,
        "--bootloader-slot",
        str(bootloader_slot),
    ]

    if serial:
        command.append("--serial")
        command.append(serial)
    if reset_only:
        command.append("--reset-only")
    if check_53_version:
        command.append("--check-nrf53-version")

    try:
        result = subprocess.run(command)
        logger.info("Output from dfu script:")
        logger.info(result.stdout)
        return result.stdout
    except subprocess.CalledProcessError as e:
        logger.error("Error from dfu script:")
        logger.error(e.stdout)
        logger.error(e.stderr)
        raise e


def pyocd_flash_device(serial, hexfile, target_type="nrf91"):
    with ConnectHelper.session_with_chosen_probe(
        unique_id=serial, target_override=target_type
    ) as session:
        programmer = FileProgrammer(session)
        programmer.program(hexfile)


def get_first_artifact_match(pattern):
    matches = glob.glob(pattern)
    if matches:
        return matches[0]
    else:
        return None
