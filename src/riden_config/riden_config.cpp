// SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
//
// SPDX-License-Identifier: MIT

#include "timezones.h"
#include <riden_config/riden_config.h>
#include <riden_logging/riden_logging.h>

#include <EEPROM.h>

#define MAGIC "RD"
#define LATEST_CONFIG_VERSION 1

using namespace RidenDongle;

// NOTE: This struct must never change
struct RidenConfigHeader {
    char magic[sizeof(MAGIC)];
    uint8_t config_version;
};

// V1 Configuration Struct
struct RidenConfigStructV1 {
    RidenConfigHeader header;
    char tz_name[100];
    bool config_portal_on_boot;
};

bool RidenConfig::begin()
{
    EEPROM.begin(512);
    RidenConfigStructV1 config{0};
    EEPROM.get(0, config);
    if (memcmp(config.header.magic, "RD", sizeof(MAGIC)) == 0 && config.header.config_version == 1) {
        tz_name = config.tz_name;
        config_portal_on_boot = config.config_portal_on_boot;
    } else {
        LOG_LN("RidenConfig: Incorrect magic");
        // Creating a default config
        tz_name = "";
        config_portal_on_boot = false;
        commit();
    }
    return true;
}

void RidenConfig::set_timezone_name(String tz_name)
{
    this->tz_name = tz_name;
}

String RidenConfig::get_timezone_name()
{
    return tz_name;
}

String RidenConfig::get_timezone_spec()
{
    for (int i = 0; i < get_number_of_timezones(); i++) {
        Timezone timezone = TIMEZONES[i];
        if (tz_name.compareTo(timezone.name) == 0) {
            return timezone.tz;
        }
    }
    return "";
}

int RidenConfig::get_number_of_timezones()
{
    return NOF_TIMEZONES;
}

const Timezone &RidenConfig::get_timezone(int index)
{
    return TIMEZONES[index];
}

void RidenConfig::set_config_portal_on_boot()
{
    config_portal_on_boot = true;
}

bool RidenConfig::get_and_reset_config_portal_on_boot()
{
    if (config_portal_on_boot) {
        config_portal_on_boot = false;
        commit();
        return true;
    }
    return false;
}

bool RidenConfig::commit()
{
    RidenConfigStructV1 config;
    memcpy(config.header.magic, MAGIC, sizeof(MAGIC));
    config.header.config_version = LATEST_CONFIG_VERSION;
    strcpy(config.tz_name, tz_name.c_str());
    config.config_portal_on_boot = config_portal_on_boot;
    EEPROM.put(0, config);
    bool success = EEPROM.commit();
    if (success) {
        LOG_LN("RidenConfig: Saved configuration");
    } else {
        LOG_LN("RidenConfig: Failed to save configuration");
    }
    return success;
}

RidenConfig RidenDongle::riden_config;
