// SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
//
// SPDX-License-Identifier: MIT

#pragma once

#include <riden_modbus/riden_modbus.h>

#include <ModbusTCP.h>

namespace RidenDongle
{

/**
 * @brief Modbus TCP bridge.
 */
class RidenModbusBridge
{
  public:
    RidenModbusBridge(RidenModbus &riden_modbus) : riden_modbus(riden_modbus){};
    bool begin();
    bool loop();

    uint16_t port();

    Modbus::ResultCode modbus_tcp_raw_callback(uint8_t *data, uint8_t len, void *custom_data);
    Modbus::ResultCode modbus_rtu_raw_callback(uint8_t *data, uint8_t len, void *custom);

  private:
    RidenModbus &riden_modbus;
    ModbusTCP modbus_tcp;
    bool initialized = false;

    // State of any currently running modbus command
    uint16_t transaction_id = 0; // ModbusTCP transaction
    uint8_t slave_id = 0;        // Request slave
    uint32_t ip = 0;
};

} // namespace RidenDongle
