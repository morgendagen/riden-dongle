#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
#
# SPDX-License-Identifier: MIT
#
# This example reads register 8 (set voltage) and
# prints the value.

import argparse
import sys
try:
    from pyModbusTCP.client import ModbusClient
except ImportError:
    print("This example requires pyModbusTCP.")
    print("To install: python3 -m pip install pyModbusTCP")
    sys.exit(1)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("hostname", help="Riden hostname, e.g. RD6006-12345678.local")
    args = parser.parse_args()
    hostname = args.hostname
    print(f'Connecting to {hostname}')
    try:
        client = ModbusClient(host=hostname, port=502, unit_id=1, auto_open=True, timeout=2)
        regs = client.read_holding_registers(8)
        if regs:
            print(regs)
        else:
            print("read error")
    except:
        print(f"Failed communicating with {hostname}")
        exit(-1)
