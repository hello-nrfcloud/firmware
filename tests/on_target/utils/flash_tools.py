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

def flash_device(hexfile):
    # hexfile (str): Full path to file (hex or zip) to be programmed
    logger.info(f"Flashing device with {hexfile}")
    try:
        result = subprocess.run(['nrfutil', 'device', 'program', '--firmware', hexfile], check=True, text=True, capture_output=True)
        logger.info("Command output:")
        logger.info(result.stdout)
        logger.info("Command completed successfully.")
    except subprocess.CalledProcessError as e:
        # Handle errors in the command execution
        logger.info("An error occurred while flashing the device.")
        logger.info("Error output:")
        logger.info(e.stderr)

    logger.info(f"Resetting device")
    try:
        result = subprocess.run(['nrfutil', 'device', 'reset'], check=True, text=True, capture_output=True)
        logger.info("Command output:")
        logger.info(result.stdout)
        logger.info("Command completed successfully.")
    except subprocess.CalledProcessError as e:
        # Handle errors in the command execution
        logger.info("An error occurred while resetting the device.")
        logger.info("Error output:")
        logger.info(e.stderr)

def reset_device():
    logger.info(f"Resetting device")
    try:
        result = subprocess.run(['nrfutil', 'device', 'reset'], check=True, text=True, capture_output=True)
        logger.info("Command output:")
        logger.info(result.stdout)
        logger.info("Command completed successfully.")
    except subprocess.CalledProcessError as e:
        # Handle errors in the command execution
        logger.info("An error occurred while resetting the device.")
        logger.info("Error output:")
        logger.info(e.stderr)
