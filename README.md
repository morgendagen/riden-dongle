# Riden Dongle - A Multi-Protocol Firmware for the Riden WiFi Module

This is an alternative firmware for the Riden WiFi module that
provides Modbus TCP and SCPI support as well as a web interface.

The firmware has been tested with various tools and libraries:

- Riden Hardware
  - RD6006
- Riden Firmware
  - Riden v1.28
  - Riden v1.41
  - Unisoft v1.41.1k
- Modbus TCP
  - [Python pyModbusTCP library](https://pypi.org/project/pyModbusTCP/)
  - [Python pymodbus library](https://pypi.org/project/pymodbus/)
  - A modified version of [ridengui](https://github.com/ShayBox/RidenGUI)
    with Modbus TCP support hacked in
- SCPI
  - [lxi-tools](https://github.com/lxi-tools/lxi-tools)
  - [EEZ Studio](https://www.envox.eu/studio/studio-introduction/)


## Features

- Modbus RTU client communicating with Riden power supply firmware.
- Modbus TCP bridge.
- SCPI control.
- Web interface to configure the dongle and update firmware.
- Automatically set power supply clock based on NTP.
- mDNS advertising.
- Handles approximately 65 queries/second using Modbus TCP or SCPI
  (tested using Unisoft v1.41.1k, UART baudrate set at 921600).


## Warning

- When flashing the Riden WiFi module you _will_ erase the existing firmware.
- The firmware provided in this repository comes with no warranty.


## Query Performance

The regular Riden power supply firmware is considerably slower than UniSoft,
handling less than 10 queries/second.


## Hardware Preparations

This one had me stuck for some time. To quote from
https://community.home-assistant.io/t/riden-rd6006-dc-power-supply-ha-support-wifi/163849:

> I had to do small modification to the wifi board - I snipped one pin
> from the pinheader (EN-Enable) so it does not make contact with the
> power supply and soldered 1k resistor between EN and 3.3V. It is done
> because the PS enables wifi module only in “wifi mode” but I need it
> to run in “TTL mode” as well.

In order to flash an existing Riden WiFi module, solder on
three additional wires: GPIO0, EN, and 3.3V. In order to ease
development you may want to terminate the wires in a Dupont header connector
allowing you to more easily use an ESP01 USB Serial Adapter or similar.


## Download the Firmware from GitHub

Firmware files will be
[released on GitHub](https://github.com/morgendagen/riden-dongle/releases)
as part of the repository.


## Compiling the Firmware

You will need [PlatformIO](https://platformio.org/) in order to
compile the
firmware.

No configuration is necessary; simply execute `pio run` and wait.
The firmware is located at `.pio/build/esp12e/firmware.bin`.


## Flashing the Firmware

Provided you have prepared the hardware as described, connect
it to your computer as you would when flashing any other ESP12F module.

Execute

    pio run -t upload --upload-port <ESP12F serial port>

and wait for the firmware to be flashed.

Before re-inserting the module into your power supply,
it may be a good idea to make the necessary configuration
changes. You need to select `TTL` as the communications mode,
and 9600 as the speed.

Re-insert the module and power up the power supply.

The module will begin to flash, first slowly and then
faster. If it starts flashing really fast (5 flashes
per second), you propably misconfigured the power supply.
Double-check, and if you are still having issues, add
an issue to the Github repository.

If all is well, the module has created a new access
point, named `RDxxxx-ssssssss` (`xxxx` is the model
and `ssssssss` is the serial number), e.g.
`RD6006-00001234`.

Connect to this access point, and you will be greeted
by a web page allowing you to configure the WiFi network
that the module should connect to.

Follow the instructions, and save the configuration.

If all goes well, the blue LED will start to flash slowly
after a short while, and eventually stop. You should now
be able to connect to the dongle at
http://RDxxxx-ssssssss.local.


## Using lxi-tools to Verify Installation

Execute the command

    lxi discover -m

to get a list of discovered SCPI devices on the network.
This firmware sneakily advertised `lxi` support in order
for lxi-tools to recognise it.

Execute the command

    lxi scpi -a RDxxxx-ssssssss.local -r "*IDN?"

to retrieve the SCPI identification string containing
power supply model, and firmware version.

Execute the command

    lxi scpi -a RDxxxx-ssssssss.local -r "VOLT?"

to retrieve the currently set voltage.

Invoke

    lxi scpi -a RDxxxx-ssssssss.local -r "VOLT 3.3"

to set the voltage to 3.3V

A description of the implemented commands is
available in [SCPI_COMMANDS.md](SCPI_COMMANDS.md).


## OTA firmware update

In order to update the firmware, you may prefer
to use OTA update instead of having to remove
the module from the power supply and connecting
it to a computer.

From the `Configure` page you can upload a
new firmware binary.


## Development

For easy development, hook up an ESP8266 NodeMCU
development board and connect RXD to D6, TXD to D5, and
GND to GND.

Finally use the `env:nodemcuv2` PlatformIO environment during
development.

This allows simultaneously having the NodeMCU connected
to the power supply and using the USB serial port for
flashing as well as monitor log output.

Beware that this configuration uses `SoftwareSerial`
for the connection to the power supply, which limits
the safe UART baudrate to 38400 bps.


## Limitations

The Riden power supply firmware has some quirks as described
below. The firmware provided here err towards caution, and
does not implement functionality that is known to be
unreliable.

### Currently Active OVP and OCP Values

There is no way to reliably retrieve these values. If they are set
by selecting a preset, M0 does not reflect the new values. If they
are set via the front panel, M0 does reflect the new values.

Therefore I have decided NOT to support `*SAV`. `*RCL` is implemented.

### Preset Register

The Preset register (19) only reflects the active preset if
changed via the modbus interface. It is not updated if a preset
is selected using the front panel. Therefore it is currently not
possible to retrieve the selected preset.

### Language Selection

Only 0 and 1 are recognised when setting the Language register. Reading
the register matches the language set from the front panel.

### Keypad

It is not possible to control keypad lock.


## Credits

- https://github.com/emelianov/modbus-esp8266
- https://github.com/sfeister/scpi-parser-arduino
- https://github.com/j123b567/scpi-parser
- https://github.com/ShayBox/Riden
- https://github.com/tzapu/WiFiManager
- https://github.com/nayarsystems/posix_tz_db
