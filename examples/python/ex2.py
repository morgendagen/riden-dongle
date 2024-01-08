#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
#
# SPDX-License-Identifier: MIT

import sys
from time import sleep
try:
    import pymodbus.client as ModbusClient
except ImportError:
    print("This example requires pymodbus.")
    print("To install: python3 -m pip install pymodbus")
    sys.exit(1)

RD_HOST = "rd6006-12345678.local"

client = ModbusClient.ModbusTcpClient(RD_HOST, port=502, timeout=2)
success = client.connect()
if not success:
    print("Failed to connect")
    sys.exit(1)
print(f"Connected")

# Continuously read the "output on" register
while True:
    read = client.read_holding_registers(address=18, slave=1)
    print(read.registers)
    sleep(1)
