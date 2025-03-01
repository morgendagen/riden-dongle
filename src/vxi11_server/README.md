# VXI handler

## Credit where credit due

This code is heavily inspired by work from https://github.com/awakephd/espBode

It is based on an extract from 2025-02-22.

Changes made:

* made platformio compatible (removed the streams)
* removed unneeded parts, simplified, compacted style
* created interfaces to riden_scpi elsewhere in this project

# About VXI-11

A VXI-11 setup is normally built upon 3 socket services:

* portmap/rpcbind (port 111, on UDP and TCP)
* vxi-11 (any TCP port you want)

Discovery is done via portmap when replying to GETPORT VXI-11 Core. It replies with a port number, normally taken out of a range of ports.

This should be on UDP and TCP, but you can get by in TCP. This implementation supports both.

Secondary discovery is done via mDNS, and the service name is "vxi-11" (_vxi-11._tcp). This however still requires the portmapper. mDNS is not handled here.

The VISA string is like: `TCPIP::<ip address>::INSTR`

The SCPI commands and responses are sent as binary data, with a header and a payload.

VXI-11 has separate commands for reading and writing, unlike scpi-raw.

This type of service is discoverable by pyvisa, and requires no special construction in Python

# Overall functionning of this code

* 2 portmap services are started, one on TCP, one on UDP, both ready to handle "PORTMAP" requests.
* 1 vxi server will be started
* The portmap services, upon reception of a PORTMAP request, and only if another client is not already connected to the vxi server, will reply with the port of the vxi server.
* The vxi service handles in essence 4 types of requests:
  * VXI_11_CREATE_LINK: accept a new client
  * VXI_11_DEV_WRITE: receive a new SCPI request from the client, and send to the SCPI device
  * VXI_11_DEV_READ: send any last data the SCPI device created to the client
  * VXI_11_DESTROY_LINK: close the connection. It can be forced to restart the vxi server on a new port, taken from a range of ports, as some clients require ports to change at each connection.
