#!/usr/bin/env python3
#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

from pyocd.core.helpers import ConnectHelper
from pyocd.flash.file_programmer import FileProgrammer
from pyocd.core.target import Target
from pyocd.target.family.target_nRF91 import ModemUpdater
from pyocd.core.exceptions import TargetError
from pyocd.debug.elf.symbols import ELFSymbolProvider
from intelhex import IntelHex
from tempfile import TemporaryDirectory
import os
from timeit import default_timer as timer
import argparse

import logging

logging.basicConfig(level=logging.INFO)

parser = argparse.ArgumentParser(
    description="Wrapper around pyOCD to fump ETB on fatal error"
)
parser.add_argument("-e", "--elf-file", help="perform mass_erase", required=True)
parser.add_argument("-o", "--output", help="output binary file", required=True)
parser.add_argument("-s", "--halting-symbol", help="symbol to set breakpoint at", default="k_sys_fatal_error_handler")
parser.add_argument("-u", "--uid", help="probe uid")

args = parser.parse_args()

options = {
    "target_override": "nrf91",
    "cmsis_dap.limit_packets": False,
    "frequency": 8000000,
    "logging": {"loggers": {"pyocd.coresight": {"level": logging.INFO}}},
}


def BIT(x):
    return 1 << x


def CS_UNLOCK(target, base):
    target.write32(base + 0xFB0, 0xC5ACCE55)


def CS_LOCK(target, base):
    target.write32(base + 0xFB0, 0)


# Embedded Trace Buffer registers
ETB_BASE_ADDR = 0xE0051000
ETB_RDP = ETB_BASE_ADDR + 0x004
ETB_STS = ETB_BASE_ADDR + 0x00C
ETB_RRD = ETB_BASE_ADDR + 0x010
ETB_RRP = ETB_BASE_ADDR + 0x014
ETB_RWP = ETB_BASE_ADDR + 0x018
ETB_TRG = ETB_BASE_ADDR + 0x01C
ETB_CTL = ETB_BASE_ADDR + 0x020
ETB_RWD = ETB_BASE_ADDR + 0x024
ETB_FFSR = ETB_BASE_ADDR + 0x300
ETB_FFCR = ETB_BASE_ADDR + 0x304

ETB_CTL_TRACECAPTEN = BIT(0)

ETB_FFSR_FLINPROG = BIT(0)
ETB_FFSR_FTSTOPPED = BIT(1)

ETB_FFCR_ENFTC = BIT(0)
ETB_FFCR_ENFCONT = BIT(1)

# Embedded Trace Macrocell registers
ETM_BASE_ADDR = 0xE0041000
ETM_TRCPRGCTLR = ETM_BASE_ADDR + 0x004  # Programming Control Register
ETM_TRCSTATR = ETM_BASE_ADDR + 0x00C  # Status Register
ETM_TRCCONFIGR = ETM_BASE_ADDR + 0x010  # Trace Configuration Register
ETM_TRCCCCTLR = ETM_BASE_ADDR + 0x038  # Cycle Count Control Register
ETM_TRCSTALLCTLR = ETM_BASE_ADDR + 0x02C  # Stall Control Register
ETM_TRCTSCTLR = ETM_BASE_ADDR + 0x030  # Timestamp Control Register
ETM_TRCTRACEIDR = ETM_BASE_ADDR + 0x040  # Trace ID Register
ETM_TRCVICTLR = ETM_BASE_ADDR + 0x080  # ViewInst Main Control Register
ETM_TRCEVENTCTL0R = ETM_BASE_ADDR + 0x020  # Event Control 0 Register
ETM_TRCEVENTCTL1R = ETM_BASE_ADDR + 0x024  # Event Control 1 Register
ETM_TRCPDSR = ETM_BASE_ADDR + 0x314  # Power down status register

ETM_TRCSTATR_IDLE = BIT(0)
ETM_TRCSTATR_PMSTABLE = BIT(1)

ETM_TRCVICTLR_TRCERR = BIT(11)
ETM_TRCVICTLR_TRCRESET = BIT(10)
ETM_TRCVICTLR_SSSTATUS = BIT(9)
ETM_TRCVICTLR_EVENT0 = BIT(0)

ETM_TRCPRGCTLR_ENABLE = BIT(0)

# Advanced Trace Bus 1 (ATB) registers
ATB_1_BASE_ADDR = 0xE005A000
ATB_1_CTL = ATB_1_BASE_ADDR + 0x000
ATB_1_PRIO = ATB_1_BASE_ADDR + 0x004

# Advanced Trace Bus 2 (ATB) registers
ATB_2_BASE_ADDR = 0xE005B000
ATB_2_CTL = ATB_2_BASE_ADDR + 0x000
ATB_2_PRIO = ATB_2_BASE_ADDR + 0x004

ATB_REPLICATOR_BASE_ADDR = 0xE0058000
ATB_REPLICATOR_IDFILTER0 = ATB_REPLICATOR_BASE_ADDR + 0x000
ATB_REPLICATOR_IDFILTER1 = ATB_REPLICATOR_BASE_ADDR + 0x004

def word_to_bytes(wrd):
    result = []
    for i in range(4):
        result.append((wrd >> (8*i)) & 0xFF)
    return bytes(result)

def etm_init(target):
    # Disable ETM to allow configuration
    target.write32(ETM_TRCPRGCTLR, 0x0)

    # Wait until ETM is idle and programmer's model is stable
    required_flags = ETM_TRCSTATR_PMSTABLE | ETM_TRCSTATR_IDLE
    while target.read32(ETM_TRCSTATR) & required_flags != required_flags:
        pass

    # Configure ETM
    target.write32(ETM_TRCCONFIGR, BIT(3))
    target.write32(ETM_TRCSTALLCTLR, 0)
    target.write32(ETM_TRCTSCTLR, 0)
    target.write32(ETM_TRCTRACEIDR, BIT(2))
    target.write32(
        ETM_TRCVICTLR,
        ETM_TRCVICTLR_TRCERR
        | ETM_TRCVICTLR_TRCRESET
        | ETM_TRCVICTLR_SSSTATUS
        | ETM_TRCVICTLR_EVENT0,
    )
    target.write32(ETM_TRCEVENTCTL0R, 0)
    target.write32(ETM_TRCEVENTCTL1R, 0)

    # Enable ETM
    target.write32(ETM_TRCPRGCTLR, ETM_TRCPRGCTLR_ENABLE)


def etm_stop(target):
    target.write32(ETM_TRCPRGCTLR, 0x0)


def atb_init(target):
    # ATB replicator
    CS_UNLOCK(target, ATB_REPLICATOR_BASE_ADDR)

    # ID filter for master port 0
    target.write32(ATB_REPLICATOR_IDFILTER0, 0xFFFFFFFF)
    # ID filter for master port 1, allowing ETM traces from CM33 to ETB
    target.write32(ATB_REPLICATOR_IDFILTER1, 0xFFFFFFFD)

    CS_LOCK(target, ATB_REPLICATOR_BASE_ADDR)

    # ATB funnel 1
    CS_UNLOCK(target, ATB_1_BASE_ADDR)

    # Set priority 1 for ports 0 and 1
    target.write32(ATB_1_PRIO, 0x00000009)

    # Enable port 0 and 1, and set hold time to 4 transactions
    target.write32(ATB_1_CTL, 0x00000303)

    CS_LOCK(target, ATB_1_BASE_ADDR)

    # ATB funnel 2
    CS_UNLOCK(target, ATB_2_BASE_ADDR)

    # Set priority 3 for port 3
    target.write32(ATB_2_PRIO, 0x00003000)

    # Enable ETM traces on port 3, and set hold time to 4 transactions
    target.write32(ATB_2_CTL, 0x00000308)

    CS_LOCK(target, ATB_2_BASE_ADDR)


def etb_init(target):
    CS_UNLOCK(target, ETB_BASE_ADDR)

    # Disable ETB
    target.write32(ETB_CTL, 0)

    # Wait for ETB formatter to stop
    while not target.read32(ETB_FFSR) & ETB_FFSR_FTSTOPPED:
        pass

    # Set ETB formatter to continuous mode
    target.write32(ETB_FFCR, ETB_FFCR_ENFCONT | ETB_FFCR_ENFTC)

    # Enable ETB
    target.write32(ETB_CTL, ETB_CTL_TRACECAPTEN)

    # Wait until the ETB Formatter has started
    while target.read32(ETB_FFSR) & ETB_FFSR_FTSTOPPED:
        pass

    CS_LOCK(target, ETB_BASE_ADDR)


def etb_stop(target):
    CS_UNLOCK(target, ETB_BASE_ADDR)

    # Disable ETB
    target.write32(ETB_CTL, 0)

    # Wait for ETB formatter to flush
    while target.read32(ETB_FFSR) & ETB_FFSR_FLINPROG:
        pass

    CS_LOCK(target, ETB_BASE_ADDR)


def etb_trace_start(target):
    # start debug clock
    target.write32(0xE0080000, 1)

    atb_init(target)
    etb_init(target)
    etm_init(target)


def etb_trace_stop(target):
    etb_stop(target)
    etm_stop(target)

def etb_data_get(target):
    result = []
    CS_UNLOCK(target, ETB_BASE_ADDR)

    last_write_pointer = target.read32(ETB_RWP)

    target.write32(ETB_RRP, last_write_pointer)

    # read out 2kb ETB buffer
    for i in range(2048//4):
        result += word_to_bytes(target.read32(ETB_RRD))

    CS_LOCK(target, ETB_BASE_ADDR)

    return result


with ConnectHelper.session_with_chosen_probe(
    unique_id=args.uid, options=options
) as session:
    target = session.target

    target.reset_and_halt()

    # set breakpoint at fault handler
    logging.info(f"Using ELF file: {args.elf_file}")
    target.elf = args.elf_file
    provider = ELFSymbolProvider(target.elf)
    halting_symbol_addr = provider.get_symbol_value(
        args.halting_symbol
    )
    if halting_symbol_addr is None:
        logging.error(f"Failed to find symbol {args.halting_symbol}")
    else:
        logging.info(f"Setting breakpoint at symbol: {args.halting_symbol} @ {hex(halting_symbol_addr)}")
        target.set_breakpoint(halting_symbol_addr)

    # start ETB trace
    logging.info("Starting ETB trace")
    etb_trace_start(target)

    # reset target
    target.resume()

    # run until breakpoint
    while not target.is_halted():
        pass

    logging.info(f"target has halted with halt reason: {target.get_halt_reason()}")
    logging.info(f"target PC: {hex(target.read_core_register('pc'))}")

    target.halt()

    # stop ETB trace
    etb_trace_stop(target)

    # get ETB data
    data = bytes(etb_data_get(target))

    with open(args.output, "wb") as f:
        f.write(data)
        print(f"ETB data saved to {args.output}")
