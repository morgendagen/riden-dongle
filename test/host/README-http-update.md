# HTTP update/restart regression tests

Run from the repository root with Python 3, g++ (Linux, ASan/UBSan), and Node.js:

```sh
python test/host/run_update_tests.py
python -m pip install -r requirements.txt
pio run -e esp12e
```

The C++ harness compiles the actual current upload, completion, reboot handlers
and main loop. HTTP, Updater, ESP restart, services and time are simulated.
It covers empty POST, begin failure, false Update.end(true) without a separate
error flag, partial writes, upload abort, successful response before restart,
a bounded stop/abort for an unacknowledged HTTP client, manual portal reboot,
repeated restart requests, millis rollover and skipped services/clock work
while restart is pending. It also checks normal loop execution.

The Node test extracts and executes the two **actual embedded** reboot scripts
with a small fake DOM/timer. It keeps navigation stalled for 20 extra seconds
and verifies that the counter stops at zero and redirects once.

To reproduce the original completion failure:

```sh
python test/host/run_update_tests.py --reproduce-upstream
```

The local history must contain `e9a060958c9dcf82f24353e36c2282b057690be4`.
**Expected: nonzero assertion failure** on an empty POST incorrectly reporting
success/restarting. The normal command verifies the fix. No test flashes a
physical device; simulated boot/restart is not proof of bootloader behavior.

## Hardware evidence and scope

The user experienced ESP not returning after an HTTP update and needing a
power cycle, then verified two successful self-updates (taxom.6 to taxom.6)
with automatic reboot after these changes. The exact cause of every earlier
failure was not instrumented; an mDNS startup failure is a possible boot stall,
not a proven explanation of those events. The infinite wait is removed so
services can still start by IP if mDNS fails.

The [fork notes](https://github.com/Taxom/riden-dongle/blob/5b457d33164399e639663d96b5ab84dcafbedf0e/FORK_NOTES.md#http-firmware-update-and-restart-taxom6)
record that hardware history. The combined taxom.9 firmware later passed the
[broader hardware test](https://github.com/Taxom/riden-dongle/blob/5b457d33164399e639663d96b5ab84dcafbedf0e/FORK_NOTES.md#final-hardware-acceptance-2026-11-09).
The isolated upstream subset has an ESP12E build and the host checks above;
it has not been independently flashed during this PR preparation.

This changes HTTP upload completion and deferred restarts, not the separate
ArduinoOTA transport. It leaves upstream clock synchronization policy as-is;
the loop merely suppresses all services once a restart has been requested.
No optional detailed tracing, diagnostics UI or serial-transport changes are
required by this PR.
