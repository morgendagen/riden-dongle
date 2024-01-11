// SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
//
// SPDX-License-Identifier: MIT

#include "timezones.h"
#include <riden_config/riden_config.h>
#include <riden_logging/riden_logging.h>

#include <EEPROM.h>

#define MAGIC "RD"
#define CURRENT_CONFIG_VERSION 2

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

// V2 Configuration Struct
struct RidenConfigStructV2 {
    RidenConfigHeader header;
    char tz_name[100];
    bool config_portal_on_boot;
    uint32_t uart_baudrate;
};

bool RidenConfig::begin()
{
    EEPROM.begin(512);
    RidenConfigHeader header{0};
    EEPROM.get(0, header);
    bool success = false;
    if (memcmp(header.magic, "RD", sizeof(MAGIC)) == 0) {
        switch (header.config_version) {
        case 1: {
            RidenConfigStructV1 config;
            EEPROM.get(0, config);
            tz_name = config.tz_name;
            config_portal_on_boot = config.config_portal_on_boot;
            success = true;
            break;
        }
        case 2: {
            RidenConfigStructV2 config;
            EEPROM.get(0, config);
            tz_name = config.tz_name;
            config_portal_on_boot = config.config_portal_on_boot;
            uart_baudrate = config.uart_baudrate;
            success = true;
            break;
        }
        default:
            success = false;
        }
    }
    if (!success) {
        LOG_LN("RidenConfig: Incorrect magic");
        // Creating a default config
        success = commit();
    }
    return success;
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

uint32_t RidenConfig::get_uart_baudrate()
{
    return uart_baudrate;
}

void RidenConfig::set_uart_baudrate(uint32_t baudrate)
{
    this->uart_baudrate = baudrate;
}

bool RidenConfig::commit()
{
    RidenConfigStructV2 config;
    memcpy(config.header.magic, MAGIC, sizeof(MAGIC));
    config.header.config_version = CURRENT_CONFIG_VERSION;
    strcpy(config.tz_name, tz_name.c_str());
    config.config_portal_on_boot = config_portal_on_boot;
    config.uart_baudrate = uart_baudrate;
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
