// SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
//
// SPDX-License-Identifier: MIT

#pragma once

#include <riden_modbus/riden_modbus.h>
#include <riden_modbus_bridge/riden_modbus_bridge.h>
#include <riden_scpi/riden_scpi.h>
#include <vxi11_server/vxi_server.h>

#include <ESP8266WebServer.h>

#define HTTP_RAW_PORT 80

namespace RidenDongle
{

class RidenHttpServer
{
  public:
    explicit RidenHttpServer(RidenModbus &modbus, RidenScpi &scpi, RidenModbusBridge &bridge, VXI_Server &vxi_server) : modbus(modbus), scpi(scpi), bridge(bridge), vxi_server(vxi_server), server(HTTP_RAW_PORT) {}
    bool begin();
    void loop(void);
    uint16_t port();

  private:
    RidenModbus &modbus;
    RidenScpi &scpi;
    RidenModbusBridge &bridge;
    VXI_Server &vxi_server;
    ESP8266WebServer server;

    void handle_root_get();
    void handle_psu_get();
    void handle_config_get();
    void handle_config_post();
    void handle_disconnect_client_post();
    void handle_reboot_dongle_get();
    void handle_firmware_update_post();
    void finish_firmware_update_post();
    void handle_lxi_identification();
    void handle_not_found();
    void handle_control_get();
    void handle_status_get();
    void handle_set_i();
    void handle_set_v();
    void handle_toggle_out();
    
    void handle_modbus_qps();
    void send_redirect_root();
    void send_redirect_self();

    void send_dongle_info();
    void send_network_info();
    void send_services();
    void send_power_supply_info();
    void send_connected_clients();

    void send_as_chunks(const char *str);
    void send_info_row(const String key, const String value);
    void send_client_row(const IPAddress &ip, const String protocol);

    const char *get_firmware_version();
    const char *get_serial_number();
};

} // namespace RidenDongle
