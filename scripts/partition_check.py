#!/usr/bin/env python3
#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

"""
Check the flash partition layout of a build against a frozen reference.

The layout is read from the final devicetree of a build (zephyr.dts). Every
partition in the reference file must exist in the devicetree, on the same flash
device, with the same offset and size. Partitions not listed in the reference
file are reported but not checked.

Reference file format, one partition per line ('#' starts a comment):

    <flash node label> <partition node label> <offset> <size>
"""

import argparse
import os
import sys


def parse_args():
    parser = argparse.ArgumentParser(
        description='Check the partition layout of zephyr.dts against a reference',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        allow_abbrev=False)
    parser.add_argument('-i', '--input', required=True, help='Final devicetree (zephyr.dts).')
    parser.add_argument('-r', '--reference', required=True, help='Reference partition layout.')
    parser.add_argument('-z', '--zephyr-base', default=os.environ.get('ZEPHYR_BASE'),
                        help='Zephyr base directory, defaults to $ZEPHYR_BASE.')
    return parser.parse_args()


def cells(node, name, default):
    prop = node.props.get(name)
    return prop.to_num() if prop else default


def reg(node):
    parent = node.parent
    addr_cells = cells(parent, '#address-cells', 2)
    size_cells = cells(parent, '#size-cells', 1)
    nums = node.props['reg'].to_nums()

    def join(words):
        value = 0
        for word in words:
            value = (value << 32) | word
        return value

    return join(nums[:addr_cells]), join(nums[addr_cells:addr_cells + size_cells])


def is_partition(node):
    return node.name.startswith('partition@') and 'reg' in node.props


def partitions(dt):
    """Return {node label: (flash label, offset, size)}, offsets relative to the flash device."""
    result = {}

    for node in dt.node_iter():
        if not is_partition(node):
            continue

        offset, size = reg(node)
        parent = node.parent
        while is_partition(parent):
            offset += reg(parent)[0]
            parent = parent.parent

        # parent is now the 'partitions' container, its parent is the flash device
        flash = parent.parent
        flash_label = flash.labels[0] if flash.labels else flash.path

        for label in node.labels:
            result[label] = (flash_label, offset, size)

    return result


def load_reference(path):
    reference = {}
    with open(path, encoding='UTF-8') as f:
        for lineno, line in enumerate(f, 1):
            line = line.split('#', 1)[0].strip()
            if not line:
                continue
            fields = line.split()
            if len(fields) != 4:
                sys.exit(f'{path}:{lineno}: expected 4 fields, got {len(fields)}')
            flash, label, offset, size = fields
            reference[label] = (flash, int(offset, 0), int(size, 0))
    return reference


def fmt(entry):
    flash, offset, size = entry
    return f'{flash:<12} 0x{offset:<10x} 0x{size:x}'


def main():
    args = parse_args()

    if not args.zephyr_base:
        sys.exit('Zephyr base not set, use --zephyr-base or $ZEPHYR_BASE')
    sys.path.insert(0, os.path.join(args.zephyr_base, 'scripts', 'dts', 'python-devicetree', 'src'))
    from devicetree import dtlib

    actual = partitions(dtlib.DT(args.input))
    reference = load_reference(args.reference)

    print(f'Partitions in {args.input}:')
    for label, entry in sorted(actual.items(), key=lambda item: (item[1][0], item[1][1])):
        print(f'  {label:<28} {fmt(entry)}')
    print()

    errors = 0
    for label, expected in reference.items():
        found = actual.get(label)
        if found is None:
            print(f'MISSING   {label:<28} expected {fmt(expected)}')
            errors += 1
        elif found != expected:
            print(f'MISMATCH  {label:<28} expected {fmt(expected)}')
            print(f'          {"":<28} found    {fmt(found)}')
            errors += 1
        else:
            print(f'OK        {label:<28} {fmt(found)}')

    if errors:
        sys.exit(f'\n{errors} partition(s) differ from {args.reference}')


if __name__ == '__main__':
    main()
