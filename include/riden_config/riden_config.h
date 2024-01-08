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

  private:
    String tz_name;
    bool config_portal_on_boot;
};

extern RidenConfig riden_config;

} // namespace RidenDongle
