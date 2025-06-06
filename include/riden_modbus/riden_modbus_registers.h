// SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
//
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

namespace RidenDongle
{

// Based on https://github.com/ShayBox/Riden/blob/master/riden/register.py
enum class Register {
    // Init
    Id = 0,
    SerialNumber_High = 1,
    SerialNumber_Low = 2,
    Firmware = 3,
    SystemTemperatureCelsius_Sign = 4,
    SystemTemperatureCelsius_Value = 5,
    SystemTemperatureFarhenheit_Sign = 6,
    SystemTemperatureFarhenheit_Value = 7,
    VoltageSet = 8,
    CurrentSet = 9,
    VoltageOut = 10,
    CurrentOut = 11,
    PowerOut_H = 12,
    PowerOut_L = 13,
    VoltageIn = 14,
    Keypad = 15,
    Protection = 16, // OVP_OCP
    OutputMode = 17, // CV_CC
    Output = 18,
    Preset = 19,
    CurrentRange = 20, // Used on RD6012p
                       // Unused/Unknown 21-31
    BatteryMode = 32,
    VoltageBattery = 33,
    ProbeTemperatureCelsius_Sign = 34,
    ProbeTemperatureCelsius_Value = 35,
    ProbeTemperatureFarhenheit_Sign = 36,
    ProbeTemperatureFarhenheit_Value = 37,
    AH_H = 38,
    AH_L = 39,
    WH_H = 40,
    WH_L = 41,
    SUBSET_END = 42, // Used to indicate the end of a subset of registers
    // Unused/Unknown 42-47
    // Date
    Year = 48,
    Month = 49,
    Day = 50,
    // Time
    Hour = 51,
    Minute = 52,
    Second = 53,
    // Unused/Unknown 54
    // Calibration
    // DO NOT CHANGE Unless you know what you're doing!
    V_OUT_ZERO = 55,
    V_OUT_SCALE = 56,
    V_BACK_ZERO = 57,
    V_BACK_SCALE = 58,
    I_OUT_ZERO = 59,
    I_OUT_SCALE = 60,
    I_BACK_ZERO = 61,
    I_BACK_SCALE = 62,
    // Unused/Unknown 63-65
    // Settings/Options
    TakeOk = 66,
    TakeOut = 67,
    PowerOnBoot = 68,
    Buzzer = 69,
    Logo = 70,
    Language = 71,
    Brightness = 72,
    // Unused/Unknown 73-79
    // Presets
    M0_V = 80,
    M0_I = 81,
    M0_OVP = 82,
    M0_OCP = 83,
    M1_V = 84,
    M1_I = 85,
    M1_OVP = 86,
    M1_OCP = 87,
    M2_V = 88,
    M2_I = 89,
    M2_OVP = 90,
    M2_OCP = 91,
    M3_V = 92,
    M3_I = 93,
    M3_OVP = 94,
    M3_OCP = 95,
    M4_V = 96,
    M4_I = 97,
    M4_OVP = 98,
    M4_OCP = 99,
    M5_V = 100,
    M5_I = 101,
    M5_OVP = 102,
    M5_OCP = 103,
    M6_V = 104,
    M6_I = 105,
    M6_OVP = 106,
    M6_OCP = 107,
    M7_V = 108,
    M7_I = 109,
    M7_OVP = 110,
    M7_OCP = 111,
    M8_V = 112,
    M8_I = 113,
    M8_OVP = 114,
    M8_OCP = 115,
    M9_V = 116,
    M9_I = 117,
    M9_OVP = 118,
    M9_OCP = 119,
    // Unused/Unknown 120-255
    SYSTEM = 256,
    // NOT REGISTERS - Magic numbers for the registers
    BOOTLOADER = 5633,
};

/**
 * @brief Convert Register to uint16_t.
 *
 * @param reg The register.
 * @return The uint16_t.
 */
constexpr uint16_t operator+(Register reg) noexcept
{
    return static_cast<uint16_t>(reg);
}

} // namespace RidenDongle
