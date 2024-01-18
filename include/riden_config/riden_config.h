// SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
//
// SPDX-License-Identifier: MIT

#pragma once

#include <Arduino.h>

namespace RidenDongle
{

struct Timezone {
    const char *name;
    const char *tz;
};

extern const char *version_string;
extern const char *build_time;

class RidenConfig
{
  public:
    bool begin();
    bool commit();
    void set_timezone_name(String tz_name);
    String get_timezone_name();
    String get_timezone_spec();
    int get_number_of_timezones();
    const Timezone &get_timezone(int index);
    void set_config_portal_on_boot();
    bool get_and_reset_config_portal_on_boot();
    uint32_t get_uart_baudrate();
    void set_uart_baudrate(uint32_t baudrate);

  private:
    String tz_name = "";
    bool config_portal_on_boot = false;
    uint32_t uart_baudrate = DEFAULT_UART_BAUDRATE;
};

extern RidenConfig riden_config;

} // namespace RidenDongle
