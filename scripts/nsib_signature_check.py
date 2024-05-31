#!/usr/bin/env python3
#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause


from intelhex import IntelHex

import hashlib
import argparse
import struct
import ecdsa

def parse_args():
    parser = argparse.ArgumentParser(
        description='Check NSIB signature of a given file',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        allow_abbrev=False)
    parser.add_argument('-i', '--input', required=True, type=argparse.FileType('r', encoding='UTF-8'),
                        help='Input hex file.')
    parser.add_argument('-p', '--public-key', required=True, type=argparse.FileType('r', encoding='UTF-8'),
                        help='Public key file (PEM).')
    parser.add_argument('-a', '--start-address', required=False, type=str, default=None, help='Start address of the image (hex).')

    args = parser.parse_args()
    return args

def check_signature(hex_file, public_key, start_address):
    public_key_bytes_expected = public_key.to_string()
    image = IntelHex(hex_file)

    signature_locations = []
    offset = 0
    while True:
        offset = image.find(b'\xde\xe6\x1e\x28\x83\x84\x51\x86', start=offset+1)
        if offset == -1:
            break
        signature_locations.append(offset)

    for offset in signature_locations:
        validation_info_version = image[offset + 8]
        info_hardware_id = image[offset + 9]
        validation_info_crypto_id = image[offset + 10]
        info_magic_compatibility_id = image[offset + 11]
        starting_address = struct.unpack('<I', image.tobinstr(start=offset + 12, size=4))[0]
        hash_bytes = image.tobinstr(start=offset + 16, size=32)
        public_key_bytes = image.tobinstr(start=offset + 48, size=64)
        signature_bytes = image.tobinstr(start=offset + 112, size=64)

        if start_address is not None and start_address != starting_address:
            raise Exception(f"Signature found at address 0x{starting_address:08x} but expected 0x{start_address:08x}")

        if public_key_bytes != public_key_bytes_expected:
            print(f"Image has signature for different public key: {public_key_bytes.hex()}")
            continue
        public_key.verify(signature_bytes, hash_bytes, hashfunc=hashlib.sha256)
        print(f"Signature verified for image starting at address 0x{starting_address:08x}")
        return
    raise Exception("No matching signature found in the image.")


def main():

    args = parse_args()
    public_key = ecdsa.VerifyingKey.from_pem(args.public_key.read())

    check_signature(args.input, public_key, int(args.start_address, 16))

if __name__ == '__main__':
    main()
