# Shared UART regression tests

These tests exercise the production RidenModbus and RidenModbusBridge against
modbus-esp8266 **4.1.0**, with simulated Arduino/Wi-Fi I/O and injected UART
replies. They require Python 3 and g++ with AddressSanitizer/UBSan (Linux).
No power supply is connected, and nothing is flashed by the host tests.

```sh
python -m pip install -r requirements.txt
pio run -e esp12e
python test/host/run_tests.py
```

If dependencies are already installed elsewhere, pass
`--modbus-lib /path/to/modbus-esp8266/src`. The runner resolves that path from
the current working directory. Use the repository root for the commands above.

The three harnesses cover:

- An in-flight TCP operation followed by another TCP/web request, transaction
  ID zero, identical transaction IDs on different same-IP sockets, timeout,
  late reply draining, CRC loss, real zero and RTU exceptions.
- Caller-buffer lifetime through a 700 ms local reply and a full library
  timeout; proper local read/write completion results; write ACK loss never
  repeating an already-applied write.
- A real ModbusTCP parser/accept loop (the Wi-Fi sockets alone are simulated),
  with two same-IP TestControllers, a third reader, web work, and reconnects.
  The stress harnesses each run 200 cycles.
- Actual web status/toggle handlers compiled with the RTU implementation:
  bounded retries of only a failed status block; unknown output never
  authorizing a toggle; lost ACK readback; an unapplied or unknowable command
  returning an error; a completed command with failed telemetry reported
  separately. Another harness runs 100 toggle cycles with TCP readers and
  injected read/ACK faults.

To reproduce the original buffer-lifetime bug with the same harness:

```sh
python test/host/run_tests.py --reproduce-upstream
```

This deliberately loads the original files from commit
`e9a060958c9dcf82f24353e36c2282b057690be4` using `git show` (no network, but
that commit must exist locally). **Expected result: nonzero exit with an
AddressSanitizer heap-use-after-free.** Run the normal command to verify the fix.
The default runner does not execute the intentionally failing reproduction.

## Hardware evidence and broader tools

The combined firmware `1.7.0-taxom.9` passed a 674-second RD6006P/UniSoft,
ESP-12F, 115200-baud acceptance run with two TCP readers, a controller and
HTTP load. See the [recorded results](https://github.com/Taxom/riden-dongle/blob/5b457d33164399e639663d96b5ab84dcafbedf0e/FORK_NOTES.md#final-hardware-acceptance-2026-11-09).
That firmware also includes separate input/update/diagnostic/clock changes;
the isolated upstream subset in this branch has host tests and an ESP12E build,
not a separate hardware flash/run.

The [desktop test tools](https://github.com/Taxom/riden-dongle/tree/5b457d33164399e639663d96b5ab84dcafbedf0e/tools)
and their READMEs are available in the fork. The full final acceptance tool
requires taxom.9's strict raw-Modbus validation and diagnostic configuration;
it is **not** a drop-in passing test for this smaller PR. It physically changes
setpoints and output. Its 10-minute run is not an endurance or loaded CC test.

Only transport bookkeeping and six inexpensive web counters are added here;
there is no detailed tracing UI, diagnostics endpoint or automatic clock change.
The 10 ms normal / 50 ms recovery quiet guards are implementation choices,
not a claimed documented UniSoft minimum polling interval. UART timeout
recovery is bounded to one repeat of a read; writes are never retried.
