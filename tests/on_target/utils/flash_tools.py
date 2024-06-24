##########################################################################################
# Copyright (c) 2024 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
##########################################################################################

import subprocess
import os
import sys
sys.path.append(os.getcwd())
from utils.logger import get_logger

logger = get_logger()

SEGGER = os.getenv('SEGGER')

def reset_device(serial=SEGGER):
    logger.info(f"Resetting device, segger: {serial}")
    try:
        result = subprocess.run(['nrfutil', 'device', 'reset', '--serial-number', serial], check=True, text=True, capture_output=True)
        logger.info("Command output:")
        logger.info(result.stdout)
        logger.info("Command completed successfully.")
    except subprocess.CalledProcessError as e:
        # Handle errors in the command execution
        logger.info("An error occurred while resetting the device.")
        logger.info("Error output:")
        logger.info(e.stderr)
        raise

def flash_device(hexfile, serial=SEGGER, extra_args=[]):
    # hexfile (str): Full path to file (hex or zip) to be programmed
    logger.info(f"Flashing device, segger: {serial}, firmware: {hexfile}")
    try:
        result = subprocess.run(['nrfutil', 'device', 'program', *extra_args, '--firmware', hexfile, '--serial-number', serial], check=True, text=True, capture_output=True)
        logger.info("Command completed successfully.")
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
        result = subprocess.run(['nrfutil', 'device', 'recover', '--serial-number', serial, '--core', core], check=True, text=True, capture_output=True)
        logger.info("Command completed successfully.")
    except subprocess.CalledProcessError as e:
        # Handle errors in the command execution
        logger.info("An error occurred while recovering the device.")
        logger.info("Error output:")
        logger.info(e.stderr)
        raise

def dfu_device(zipfile, serial):
    logger.info(f"Performing MCUBoot DFU, firmware: {zipfile}")
    thingy91x_dfu = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'thingy91x_dfu.py')
    # call thingy91x-dfu
    try:
        result = subprocess.run(['python3', thingy91x_dfu, '--serial', serial, '--debug', '--image', zipfile], check=True, text=True, capture_output=True)
        logger.info("Command output:")
        logger.info(result.stdout)
        logger.info(result.stderr)
        logger.info("Command completed successfully.")
    except subprocess.CalledProcessError as e:
        # Handle errors in the command execution
        logger.info("An error occurred while flashing the device.")
        logger.info("Error output:")
        logger.info(e.stderr)
        raise

def setup_jlink(serial):
    # run command and check if it was successful
    try:
        result = subprocess.run(['/opt/setup-jlink/setup-jlink.bash', serial], check=True, text=True, capture_output=False)
        logger.info("Command completed successfully.")
    except subprocess.CalledProcessError as e:
        # Handle errors in the command execution
        logger.info("An error occurred while setting up JLink.")
        logger.info("Error output:")
        logger.info(e.stderr)
        raise
