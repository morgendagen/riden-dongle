#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
#
# SPDX-License-Identifier: MIT
#
# This example connects to the power supply using
# PyVISA and PyVISA-py.

import argparse
import sys
try:
    import pyvisa
except ImportError:
    print("This example requires PyVISA and PyVISA-py.")
    print("To install: python3 -m pip install pyvisa pyvisa-py")
    sys.exit(1)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("resource", help="VISA resource address")
    args = parser.parse_args()
    resource = args.resource

    resource_manager = pyvisa.ResourceManager("@py")

    instrument = resource_manager.open_resource(resource)
    instrument.read_termination = '\n'
    instrument.write_termination = '\n'
    print(instrument.query('*IDN?'))
