#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
#
# SPDX-License-Identifier: MIT
#
# This example measures the number of Modbus TCP queries
# that can be handled per second by reading register 8
# (set voltage) continuously.

import argparse
import sys
try:
    import pymodbus.client as ModbusClient
except ImportError:
    print("This example requires pymodbus.")
    print("To install: python3 -m pip install pymodbus")
    sys.exit(1)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("hostname", help="Riden hostname, e.g. RD6006-12345678.local")
    args = parser.parse_args()
    hostname = args.hostname

    print(f'Connecting to {hostname}')
    client = ModbusClient.ModbusTcpClient(hostname, port=502, timeout=2)
    success = client.connect()
    if not success:
        print(f"Failed connecting to {hostname}")
        sys.exit(-1)
    print(f"Connected")

    try:
        # Continuously read the "output on" register
        import time
        st = time.time()
        cnt = 0
        while True:
            read = client.read_holding_registers(address=8, slave=1)
            print(read.registers)
            cnt += 1
            print(cnt / (time.time() - st))
    except KeyboardInterrupt:
        pass
    except:
        print(f"Failed communicating with {hostname}")
        exit(-1)
