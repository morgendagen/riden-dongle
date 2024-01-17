# Examples

This directory contains various examples on the usage of the firmware.


## Python

### pyModbusTCP (examples/python/ex1.py)

This script uses [pyModbusTCP](https://pypi.org/project/pyModbusTCP/) to
communicate with the power supply using Modbus TCP.

    $ ./examples/python/ex1.py RD6006-12345678.local


### pymodbus (examples/python/ex2.py)

This script uses [pymodbus](https://pypi.org/project/pymodbus/) to
communicate with the power supply using Modbus TCP.

    $ ./examples/python/ex2.py RD6006-12345678.local

## lxi-tools

### Queries per Second (examples/lxi-tools/qps.lua)

This script continuously executes a `VOLT?` query for 30 seconds, and
then calculates the average number of queries per second achieved.

    $ RIDEN_HOSTNAME="RD6006-12345678.local" lxi run examples/lxi-tools/qps.lua
