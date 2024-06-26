#!/usr/bin/env python3
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0

import sys
import usb.core
import usb.util
import argparse
import logging as default_logging
import serial
import serial.tools.list_ports
import os
import time
from tempfile import TemporaryDirectory
from zipfile import ZipFile
import re
import json
import imgtool.image

# Workaround for pyusb on windows
# libusb1 backend is used on all platforms,
# but on windows, the library DLL is not found automatically
if sys.platform == "win32":
    import usb.backend.libusb1
    import libusb
    if usb.backend.libusb1.get_backend() is None:
        usb.backend.libusb1.get_backend(find_library=lambda x : libusb.dll._name)

RECOVER_NRF53 = b"\x8e"  # connectivity_bridge bootloader mode
RECOVER_NRF91 = b"\x8f"  # nrf91 bootloader mode
RESET_NRF53 = b"\x90"  # reset nrf53
RESET_NRF91 = b"\x91"  # reset nrf91
READ_NRF53_VERSION = b"\x92"  # read nrf53 version


def add_args_to_parser(parser, default_chip=""):
    # define argument to decide whether to recover nrf53 or nrf91
    parser.add_argument(
        "--chip",
        type=str,
        help="bootloader mode to trigger: nrf53 or nrf91",
        default=default_chip,
    )
    parser.add_argument("--vid", type=int, help="vendor id", default=0x1915)
    parser.add_argument("--pid", type=int, help="product id", default=0x910A)
    parser.add_argument("--serial", type=str, help="serial number", default=None)
    parser.add_argument(
        "--debug", action="store_true", help="enable debug logging", default=False
    )
    parser.add_argument("--bootloader_slot", type=int, help="bootloader slot", default=1)


def find_bulk_interface(device, descriptor_string, logging):
    for cfg in device:
        for intf in cfg:
            # Attempt to detach kernel driver (only necessary on Linux)
            if sys.platform == "linux" and device.is_kernel_driver_active(
                intf.bInterfaceNumber
            ):
                try:
                    device.detach_kernel_driver(intf.bInterfaceNumber)
                except usb.core.USBError as e:
                    logging.error(f"Could not detach kernel driver: {str(e)}")
            if usb.util.get_string(device, intf.iInterface) == descriptor_string:
                return intf
    return None

def find_usb_device_posix(vid, pid, logging, serial_number=None):
    if serial_number is not None:
        device = usb.core.find(idVendor=vid, idProduct=pid, serial_number=serial_number)
        if device is None:
            logging.error(f"Device with serial number {serial_number} not found")
            devices = list(usb.core.find(find_all=True, idVendor=vid, idProduct=pid))
            logging.error(f"{len(devices)} available devices:")
            for device in devices:
                serial_number = usb.util.get_string(device, device.iSerialNumber)
                logging.error(f"Serial Number: {serial_number}")
            return None, None
        return device, serial_number
    devices = list(usb.core.find(find_all=True, idVendor=vid, idProduct=pid))
    if len(devices) == 1:
        device = devices[0]
        serial_number = usb.util.get_string(device, device.iSerialNumber)
        return device, serial_number
    if len(devices) == 0:
        logging.error("No devices found.")
    else:
        logging.info("Multiple devices found.")
        for device in devices:
            serial_number = usb.util.get_string(device, device.iSerialNumber)
            logging.info(f"Serial Number: {serial_number}")
        logging.warning(
            "Please specify the serial number with the --serial option."
        )
    return None, None

def find_usb_device_windows(vid, pid, logging, serial_number=None):
    devices = list(usb.core.find(find_all=True, idVendor=vid, idProduct=pid))
    if len(devices) == 1:
        coms = serial.tools.list_ports.comports()
        for com in coms:
            if com.vid == vid and com.pid == pid:
                if serial_number is not None and serial_number != com.serial_number:
                    logging.error(
                        f"Device with serial number {serial_number} not found"
                    )
                    return None, None
                return devices[0], com.serial_number
    if len(devices) == 0:
        logging.error("No devices found.")
    else:
        # On Windows, we cannot reliably get the serial number from the device.
        # Therefore, only one device can be connected at a time.
        logging.info("Multiple devices found. Please make sure only one device is connected.")
    return None, None

def find_usb_device(vid, pid, logging, serial_number=None):
    if sys.platform == "win32":
        return find_usb_device_windows(vid, pid, logging, serial_number)
    else:
        return find_usb_device_posix(vid, pid, logging, serial_number)

def prepare_bulk_endpoints(vid, pid, chip, logging, serial_number=None):
    device, serial_number = find_usb_device(vid, pid, logging, serial_number)

    if device is None:
        return

    if "THINGY91X" not in serial_number:
        logging.warning("Device is already in bootloader mode")
        if chip != "nrf53":
            logging.error(
                "The device is in nRF53 bootloader mode, " + \
                "but you are trying to flash an nRF91 image. " + \
                "Please program the connectivity_bridge firmware first."
            )
            return
        return None, None, None, serial_number

    # Find the bulk interface
    bulk_interface = find_bulk_interface(device, "CMSIS-DAP v2", logging)
    if bulk_interface is None:
        logging.error("Bulk interface not found")
        return

    # Find the out endpoint
    out_endpoint = None
    for endpoint in bulk_interface:
        if usb.util.endpoint_direction(endpoint.bEndpointAddress) == usb.util.ENDPOINT_OUT:
            out_endpoint = endpoint
            break
    if out_endpoint is None:
        logging.error("OUT endpoint not found")
        return

    # Find the in endpoint
    in_endpoint = None
    for endpoint in bulk_interface:
        if usb.util.endpoint_direction(endpoint.bEndpointAddress) == usb.util.ENDPOINT_IN:
            in_endpoint = endpoint
            break
    if in_endpoint is None:
        logging.error("IN endpoint not found")
        return

    return device, out_endpoint, in_endpoint, serial_number

def read_nrf53_version(vid, pid, chip, logging, serial_number=None):
    device, out_endpoint, in_endpoint, serial_number = \
        prepare_bulk_endpoints(vid, pid, chip, logging, serial_number)

    try:
        out_endpoint.write(READ_NRF53_VERSION)
        data = bytes(in_endpoint.read(in_endpoint.wMaxPacketSize))
        if len(data) < 2 or data[0] != READ_NRF53_VERSION[0] or data[1] == 0xFF:
            logging.error("Failed to read nRF53 version")
            return
        version_string = data[2:data[2]+2].decode("ascii")
        logging.info(f"nRF53 version: {version_string}")

    except usb.core.USBError as e:
        logging.error(f"Failed to send data: {e}")
        return

def trigger_bootloader(vid, pid, chip, logging, reset_only, serial_number=None):
    device, out_endpoint, in_endpoint, serial_number = \
        prepare_bulk_endpoints(vid, pid, chip, logging, serial_number)

    if reset_only:
        if device is None:
            return
        if chip == "nrf53":
            logging.info("Trying to reset nRF53...")
            try:
                out_endpoint.write(RESET_NRF53)
                logging.debug("Data sent successfully.")
            except usb.core.USBError as e:
                logging.error(f"Failed to send data: {e}")
                return
        else:
            logging.info("Trying to reset nRF91...")
            try:
                out_endpoint.write(RESET_NRF91)
                logging.debug("Data sent successfully.")
            except usb.core.USBError as e:
                logging.error(f"Failed to send data: {e}")
                return
            # Read the response
            try:
                data = in_endpoint.read(in_endpoint.wMaxPacketSize)
                logging.debug(f"Response: {data}")
            except usb.core.USBError as e:
                logging.error(f"Failed to read data: {e}")
        usb.util.dispose_resources(device)
        return serial_number

    if device is None:
        return serial_number

    # for nrf91, check if the first serial port of the device is available
    # for nrf53, the device will re-enumerate anyway
    if chip == "nrf91":
        found_ports = []
        for port in serial.tools.list_ports.comports():
            if port.serial_number == serial_number:
                found_ports.append(port.device)
        if len(found_ports) != 2:
            logging.error("No serial ports with that serial number found")
            sys.exit(1)
        found_ports.sort()
        try:
            with serial.Serial(found_ports[0], 115200, timeout=1):
                logging.debug("Serial port opened")
        except serial.SerialException as e:
            logging.error(f"Failed to open serial port, do you have a serial terminal open? {e}")
            sys.exit(1)

    # Send the command to trigger the bootloader
    if chip == "nrf53":
        logging.info("Trying to put nRF53 in bootloader mode...")
        try:
            out_endpoint.write(RECOVER_NRF53)
            logging.debug("Data sent successfully.")
        except usb.core.USBError as e:
            logging.error(f"Failed to send data: {e}")
            return
        # Device should be in bootloader mode now, no need to read the response
    else:
        logging.info("Trying to put nRF91 in bootloader mode...")
        try:
            out_endpoint.write(RECOVER_NRF91)
            logging.debug("Data sent successfully.")
        except usb.core.USBError as e:
            logging.error(f"Failed to send data: {e}")
            return
        # Read the response
        try:
            data = in_endpoint.read(in_endpoint.wMaxPacketSize)
            logging.debug(f"Response: {data}")
        except usb.core.USBError as e:
            logging.error(f"Failed to read data: {e}")
    usb.util.dispose_resources(device)
    return serial_number


def wait_for_nrf53_recovery_mode(serial_number, logging, enter_mcuboot=True):
    # extract the hexadecimal part of the serial number
    match = re.search(r"(THINGY91X_)?([A-F0-9]+)", serial_number)
    if match is None:
        logging.error("Serial number doesn't match expected format")
        sys.exit(1)
    serial_number_digits = match.group(2)

    port_info = None
    logging.debug("Waiting for device to enumerate...")
    for _ in range(300):
        time.sleep(0.1)
        try:
            com_ports = serial.tools.list_ports.comports()
        except TypeError:
            continue
        for port_info in com_ports:
            if port_info.serial_number is None:
                continue
            logging.debug(
                f"Serial port: {port_info.device} has serial number: {port_info.serial_number}"
            )
            if serial_number_digits in port_info.serial_number:
                logging.debug(f"Serial port: {port_info.device}")
                serial_number = port_info.serial_number
                break
        if port_info is None:
            continue
        if enter_mcuboot and ("THINGY91X" not in serial_number):
            break
        if not enter_mcuboot and ("THINGY91X" in serial_number):
            break
    if port_info is None:
        logging.error("MCUBoot serial port not found")
        sys.exit(1)
    return serial_number, port_info.device

def perform_dfu(pid, vid, serial_number, image, chip, logging):

    if chip == "nrf53":
        serial_number, port = wait_for_nrf53_recovery_mode(serial_number, logging)

    # Assemble DFU update command:
    command = (
        f"nrfutil device program --serial-number {serial_number} --firmware {image}"
    )

    logging.debug(f"Executing command: {command}")
    os.system(command)


def perform_bootloader_dfu(pid, vid, serial_number, image, chip, logging, slot=1):

    port = None
    if chip == "nrf53":
        serial_number, port = wait_for_nrf53_recovery_mode(serial_number, logging)
    else:
        ports = []
        com_ports = serial.tools.list_ports.comports()
        for port_info in com_ports:
            if port_info.serial_number == serial_number:
                ports.append(port_info.device)
        ports.sort()
        port = ports[0]

    with TemporaryDirectory() as tmpdir:
        with ZipFile(image, "r") as zip_ref:
            zip_ref.extractall(tmpdir)
        with open(os.path.join(tmpdir, "manifest.json")) as manifest:
            manifest = json.load(manifest)
        imgfile = os.path.join(tmpdir, manifest["files"][slot]["file"])

        # calculate digest
        ret, version, digest = imgtool.image.Image.verify(imgfile, None)
        if ret != imgtool.image.VerifyResult.OK:
            logging.error(f"Image verification failed: {ret}")
            sys.exit(1)
        digest = digest.hex()

        # prime serial connection (nrf91 only)
        if chip == "nrf91":
            os.system(f"mcumgr --conntype serial  --connstring {port} image list -t 1 || true")
        # list images (just to check if mcuboot is running)
        ret = os.system(f"mcumgr --conntype serial  --connstring {port} image list")
        if ret != 0:
            logging.error("Failed to list mcuboot images")
            sys.exit(1)
        # upload image to secondary slot
        ret = os.system(f"mcumgr --conntype serial  --connstring {port} image upload {imgfile} -n 2")
        if ret != 0:
            logging.error("Failed to upload image")
            sys.exit(1)
        # confirm image
        ret = os.system(f"mcumgr --conntype serial  --connstring {port} image confirm {digest}")
        if ret != 0:
            logging.error("Failed to confirm image")
            sys.exit(1)
        # reset device
        ret = os.system(f"mcumgr --conntype serial  --connstring {port} reset")
        if ret != 0:
            logging.error("Failed to reset device")
            sys.exit(1)

        # wait until device is done rebooting (old bootloader loads new bootloader)
        if chip == "nrf53":
            serial_number, port = wait_for_nrf53_recovery_mode(serial_number[:11], logging, enter_mcuboot=False)
        if chip == "nrf91":
            time.sleep(5) # don't have a good way to detect when the device is ready
        # reboot device (new bootloader)
        trigger_bootloader(vid, pid, chip, logging, reset_only=True, serial_number=serial_number)


def detect_family_from_zip(zip_file, logging):
    is_mcuboot = False
    with TemporaryDirectory() as tmpdir:
        with ZipFile(zip_file, "r") as zip_ref:
            zip_ref.extractall(tmpdir)
        try:
            with open(os.path.join(tmpdir, "manifest.json")) as manifest:
                manifest_content = manifest.read()
        except FileNotFoundError:
            logging.error("Manifest file not found")
            return None, is_mcuboot
    if '"type": "mcuboot"' in manifest_content:
        is_mcuboot = True
    if "nrf53" in manifest_content:
        return "nrf53", is_mcuboot
    if "nrf91" in manifest_content:
        return "nrf91", is_mcuboot
    return None, is_mcuboot


def main(args, reset_only, logging=default_logging):
    # if logging does not contain .error function, map it to .err
    if not hasattr(logging, "error"):
        logging.debug = logging.dbg
        logging.info = logging.inf
        logging.warning = logging.wrn
        logging.error = logging.err

    if hasattr(args, 'check_nrf53_version') and args.check_nrf53_version:
        read_nrf53_version(args.vid, args.pid, args.chip, logging, args.serial)
        sys.exit(0)

    # determine chip family
    chip = args.chip
    if hasattr(args, 'image') and args.image is not None:
        # if image is provided, try to determine chip family from image
        chip, is_mcuboot = detect_family_from_zip(args.image, logging)
        if chip is None:
            logging.error("Could not determine chip family from image")
            sys.exit(1)
    if len(args.chip) > 0 and args.chip != chip:
        logging.error("Chip family does not match image")
        sys.exit(1)
    if chip not in ["nrf53", "nrf91"]:
        logging.error("Invalid chip")
        sys.exit(1)

    serial_number = trigger_bootloader(args.vid, args.pid, chip, logging, reset_only, args.serial)
    if serial_number is not None:
        if reset_only:
            logging.info(f"{chip} on {serial_number} has been reset")
        else:
            logging.info(f"{chip} on {serial_number} is in bootloader mode")
    else:
        sys.exit(1)

    # Only continue if an image file is provided
    if hasattr(args, 'image') and args.image is not None:
        if is_mcuboot:
            perform_bootloader_dfu(args.pid, args.vid, serial_number, args.image, chip, logging, args.bootloader_slot)
        else:
            perform_dfu(args.pid, args.vid, serial_number, args.image, chip, logging)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Thingy91X DFU", allow_abbrev=False)
    parser.add_argument("--image", type=str, help="application update file")
    parser.add_argument("--check-nrf53-version", action="store_true", help="check nrf53 version")
    add_args_to_parser(parser)
    args = parser.parse_args()

    default_logging.basicConfig(
        level=default_logging.DEBUG if args.debug else default_logging.INFO
    )

    main(args, reset_only=False, logging=default_logging)
