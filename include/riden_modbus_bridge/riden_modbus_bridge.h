// SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
//
// SPDX-License-Identifier: MIT

#pragma once

#include <riden_modbus/riden_modbus.h>

// ModbusSettings.h is already included by ModbusRTU. Disable the TCP
// template's IP-only connection eviction before including its definition.
// Two TestController processes on one PC must keep separate sockets.
#undef MODBUSIP_UNIQUE_CLIENTS
#include <ModbusTCP.h>
#include <list>

namespace RidenDongle
{

class RidenModbusTCP : public ModbusTCP
{
  public:
    /**
     * @brief Get a list of the connected clients.
     */
    std::list<IPAddress> get_connected_clients();

    /**
     * @brief Disconnect a client.
     *
     * @param ip IP-address of client to disconnect.
     */
    void disconnect_client(const IPAddress &ip);

    // Send without using ModbusTCP's shared receive/PDU buffer. A UART reply
    // may arrive while task() is processing the next incoming TCP request.
    int8_t current_client() const { return n; }
    bool send_response(int8_t client, uint16_t transaction, uint8_t unit,
                       const uint8_t *data, uint8_t len);
};

/**
 * @brief Modbus TCP bridge.
 */
class RidenModbusBridge
{
  public:
    explicit RidenModbusBridge(RidenModbus &riden_modbus) : riden_modbus(riden_modbus){};
    bool begin();
    bool loop();

    uint16_t port();
    std::list<IPAddress> get_connected_clients();
    void disconnect_client(const IPAddress &ip);

    Modbus::ResultCode modbus_tcp_raw_callback(uint8_t *data, uint8_t len, void *custom_data);
    Modbus::ResultCode modbus_rtu_raw_callback(uint8_t *data, uint8_t len, void *custom);

    struct Diagnostics {
        uint32_t requests = 0;
        uint32_t read_retries = 0;
        uint32_t recovered_reads = 0;
        uint32_t uart_timeouts = 0;
        uint32_t invalid_responses = 0;
        uint32_t uart_exceptions = 0;
        uint32_t gateway_failures = 0;
        uint16_t last_failed_register = 0;
        uint16_t last_failure_code = 0;
        uint8_t last_failed_function = 0;
    };
    const Diagnostics &get_diagnostics() const { return diagnostics; }

  private:
    RidenModbus &riden_modbus;
    RidenModbusTCP modbus_tcp;
    bool initialized = false;
    Diagnostics diagnostics;

    void finish_request();
    void fail_request(Modbus::ResultCode error);
    bool valid_response(const uint8_t *data, uint8_t len) const;

    // State of any currently running modbus command
    bool request_pending = false;
    uint8_t request_header[5] = {};
    uint8_t request_length = 0;
    uint16_t transaction_id = 0; // ModbusTCP transaction
    uint8_t slave_id = 0;        // Request slave
    int8_t client_slot = -1;
};

} // namespace RidenDongle
