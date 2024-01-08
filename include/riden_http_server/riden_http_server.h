// SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
//
// SPDX-License-Identifier: MIT

#pragma once

#include <riden_modbus/riden_modbus.h>
#include <riden_modbus_bridge/riden_modbus_bridge.h>
#include <riden_scpi/riden_scpi.h>

#include <ESP8266WebServer.h>

#define HTTP_RAW_PORT 80

namespace RidenDongle
{

class RidenHttpServer
{
  public:
    RidenHttpServer(RidenModbus &modbus, RidenScpi &scpi, RidenModbusBridge &bridge) : modbus(modbus), scpi(scpi), bridge(bridge), server(HTTP_RAW_PORT) {}
    bool begin();
    void loop(void);

  private:
    RidenModbus &modbus;
    RidenScpi &scpi;
    RidenModbusBridge &bridge;
    ESP8266WebServer server;

    void get_root();
    void post_root();
    void handle_get_psu();
    void get_config();
    void post_config();
    void reboot_dongle();
    void handle_not_found();
    void send_redirect_self();

    void send_connected_clients();
    void send_network_info();
    void send_services();
    void send_power_supply_info();

    void send_as_chunks(const char *str);
    void send_info_row(String key, String value);
};

} // namespace RidenDongle
