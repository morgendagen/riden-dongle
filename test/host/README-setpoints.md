# HTTP setpoint validation regression tests

Run from the repository root with Python 3 and g++ (Linux, ASan/UBSan):

```sh
python test/host/run_setpoint_tests.py
python -m pip install -r requirements.txt
pio run -e esp12e
```

The runner extracts the **actual current** HTTP setter functions and numeric
write helpers/conversions into a small facade. HTTP and register I/O are
simulated; these are unit tests of the validation boundary, not an RTU timing
or ESP networking simulation.

Covered cases include empty input, whitespace-only input, text/trailing units,
comma decimals, NaN/Inf, negative values, overflow/underflow, hexadecimal syntax,
embedded NUL, above-model limits and invalid/unsupported current ranges.
Rejected commands must return HTTP 400 without a register write. Valid zero,
exact limits, surrounding whitespace and decimal exponents must reach the
correct voltage/current registers. Direct callers are checked for non-finite,
negative and unrepresentable values; a reported current-range read failure
must stop the write. Connection/write failures remain HTTP 500.

The last test injects a false range-read result at the register-I/O boundary;
correct propagation of an actual RTU timeout is a separate transport concern
addressed by the shared-UART PR. No automatic write retry is introduced.

Reproduce the first failure with the original setters/helpers:

```sh
python test/host/run_setpoint_tests.py --reproduce-upstream
```

The local history must contain `e9a060958c9dcf82f24353e36c2282b057690be4`.
**Expected: nonzero assertion failure** because an empty POST is treated as a
valid zero and writes a register. The normal command verifies the fixed code.

## Scope and hardware evidence

This is independently based on upstream main. It validates the HTTP setters and
shared numeric write helpers; it does **not** add validation of arbitrary raw
Modbus TCP PDUs. That larger bridge policy is available separately in the fork.
No calibration, preset, clock or persistent configuration is changed by tests.

The combined firmware taxom.9 passed [hardware acceptance](https://github.com/Taxom/riden-dongle/blob/5b457d33164399e639663d96b5ab84dcafbedf0e/FORK_NOTES.md#final-hardware-acceptance-2026-11-09),
including 29 rejected HTTP and 13 rejected Modbus commands in each diagnostic
mode, real-zero writes and unchanged protected settings. The isolated subset
in this branch has host tests and an ESP12E build, not a separate hardware run.

The [full PC acceptance tool and README](https://github.com/Taxom/riden-dongle/tree/5b457d33164399e639663d96b5ab84dcafbedf0e/tools/final_acceptance_test)
require the combined taxom.9 firmware, including raw-Modbus validation and
diagnostic mode switching. They intentionally refuse an older firmware before
writing. They physically change output/V/I, so they are not run by host CI.
