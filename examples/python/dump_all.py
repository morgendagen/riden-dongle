#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
#
# SPDX-License-Identifier: MIT

import sys
try:
    from pyModbusTCP.client import ModbusClient
except ImportError:
    print("This example requires pyModbusTCP.")
    print("To install: python3 -m pip install pyModbusTCP")
    sys.exit(1)

RD_HOST = "192.168.4.177"

c = ModbusClient(host=RD_HOST, port=502, unit_id=1, auto_open=True, timeout=2)
nr = 16
print("address;value")
for a in range(0, 1023, nr):
    regs = c.read_holding_registers(a, reg_nb=nr)
    for x in range(0, nr):
        if (regs is not None) and (x < len(regs)):
            v = regs[x]
        else:
            v = None
        print(f"{a + x};{v}")
