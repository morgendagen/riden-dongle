# Developing the Riden Dongle Firmware

## Development Hardware

For easy development, hook up an ESP8266 NodeMCU
development board and connect RXD to D6, TXD to D5 and
GND to GND.

Finally use the `env:nodemcuv2` PlatformIO environment during
development.

This allows simultaneously having the NodeMCU connected
to the power supply and using the USB serial port for
flashing as well as monitor log output.

Beware that this configuration uses `SoftwareSerial`
for the connection to the power supply, which limits
the safe UART baudrate to 38400 bps.


## Testing GitHub Workflow Locally

### Prerequisites

- [act](https://github.com/nektos/act)

### Testing the Release Workflow

To test the release workflow execute:

    $ act --container-architecture linux/amd64 --artifact-server-path $PWD/act-cache -e act/release.json -W .github/workflows/release.yaml
