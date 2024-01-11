// SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
//
// SPDX-License-Identifier: MIT

#pragma once

#include "riden_modbus_registers.h"

#include <ModbusRTU.h>
#include <WString.h>
#include <stdint.h>
#include <time.h>

#define MODBUS_ADDRESS 1
#define NUMBER_OF_PRESETS 9

namespace RidenDongle
{

enum class Protection {
    OVP = 1,
    OCP = 2,
    None = 0xff,
};

enum class OutputMode {
    CONSTANT_VOLTAGE = 0,
    CONSTANT_CURRENT = 1,
    Unknown = 0xff,
};

struct Preset {
    double voltage;
    double current;
    double over_voltage_protection;
    double over_current_protection;
};

struct Calibration {
    uint16_t V_OUT_ZERO;
    uint16_t V_OUT_SCALE;
    uint16_t V_BACK_ZERO;
    uint16_t V_BACK_SCALE;
    uint16_t I_OUT_ZERO;
    uint16_t I_OUT_SCALE;
    uint16_t I_BACK_ZERO;
    uint16_t I_BACK_SCALE;
};

struct AllValues {
    double system_temperature_celsius;
    double system_temperature_fahrenheit;
    double voltage_set;
    double current_set;
    double voltage_out;
    double current_out;
    double power_out;
    double voltage_in;
    bool keypad_locked;
    Protection protection;
    OutputMode output_mode;
    bool output_on;
    uint16_t current_range;
    bool is_battery_mode;
    double voltage_battery;
    double probe_temperature_celsius;
    double probe_temperature_fahrenheit;
    double ah;
    double wh;
    tm clock;
    Calibration calibration;
    bool is_take_ok;
    bool is_take_out;
    bool is_power_on_boot;
    bool is_buzzer_enabled;
    bool is_logo;
    uint16_t language;
    uint8_t brightness;
    // NOTE: Presets are zero-based, i.e. `presets[0]` refers to `M1`.
    Preset presets[NUMBER_OF_PRESETS];
};

/**
 * @brief Serial modbus connection to Riden power supply.
 */
class RidenModbus
{
  public:
    friend class RidenModbusBridge;

    bool begin();
    bool loop();

    bool is_connected();

    String get_type();
    bool get_all_values(AllValues &all_values);

    bool get_id(uint16_t &id);
    bool get_serial_number(uint32_t &serial_number);
    bool get_firmware_version(uint16_t &firmware_version);

    bool get_system_temperature_celsius(double &temperature);
    bool get_system_temperature_fahrenheit(double &temperature);

    bool get_voltage_set(double &voltage);
    bool set_voltage_set(const double voltage);

    bool get_current_set(double &current);
    bool set_current_set(const double current);

    bool get_voltage_out(double &voltage);
    bool get_current_out(double &current);

    bool get_power_out(double &power);

    bool get_voltage_in(double &voltage_in);

    bool is_keypad_locked(bool &keypad);

    bool get_protection(Protection &protection);
    bool get_output_mode(OutputMode &output_mode);

    bool get_output_on(bool &result);
    bool set_output_on(const bool on);

    /**
     * @brief Set the preset
     *
     * @param index One-based index, i.e. `1` refers to `M1`.
     * @return true On success.
     * @return false On failure.
     */
    bool set_preset(const uint8_t index);

    bool get_current_range(uint16_t &current_range);

    bool is_battery_mode(bool &battery_mode);

    bool get_voltage_battery(double &voltage_battery);

    bool get_probe_temperature_celsius(double &temperature);
    bool get_probe_temperature_fahrenheit(double &temperature);

    bool get_ah(double &ah);
    bool get_wh(double &wh);

    bool get_clock(tm &time);
    bool set_clock(const tm time);
    bool set_date(const uint16_t year, const uint16_t month, const uint16_t day);
    bool set_time(const uint8_t hour, const uint8_t minute, const uint8_t second);

    // Options

    bool is_take_ok(bool &take_ok);
    bool set_take_ok(const bool take_ok);
    bool is_take_out(bool &take_out);
    bool set_take_out(const bool take_out);
    bool is_power_on_boot(bool &power_on_boot);
    bool set_power_on_boot(const bool power_on_boot);
    bool is_buzzer_enabled(bool &buzzer);
    bool set_buzzer_enabled(const bool buzzer);
    bool is_logo(bool &logo);
    bool set_logo(const bool logo);
    bool get_language(uint16_t &language);
    bool set_language(const uint16_t language);
    bool get_brightness(uint8_t &brightness);
    bool set_brightness(const uint8_t brightness);

    // Calibration
    bool get_calibration(Calibration &calibration);
    bool set_calibration(const Calibration calibration);

    // Presets
    /**
     * @brief Store preset at `index`.
     *
     * @param index One-based index, i.e. `1` refers to `M1`.
     * @return true On success.
     * @return false On failure.
     */
    bool set_preset(const uint8_t index, const Preset &preset);

    /**
     * @brief Retrieve preset at `index`.
     *
     * @param index One-based index, i.e. `1` refers to `M1`.
     * @return true On success.
     * @return false On failure.
     */
    bool get_preset(const uint8_t index, Preset &preset);

    /**
     * @brief Store preset voltage at `index`.
     *
     * @param index One-based index, i.e. `1` refers to `M1`.
     * @return true On success.
     * @return false On failure.
     */
    bool set_preset_voltage_out(const uint8_t index, const double voltage);

    /**
     * @brief Retrive preset voltage at `index`.
     *
     * @param index One-based index, i.e. `1` refers to `M1`.
     * @return true On success.
     * @return false On failure.
     */
    bool get_preset_voltage_out(const uint8_t index, double &voltage);

    /**
     * @brief Store preset current at `index`.
     *
     * @param index One-based index, i.e. `1` refers to `M1`.
     * @return true On success.
     * @return false On failure.
     */
    bool set_preset_current_out(const uint8_t index, const double current);

    /**
     * @brief Retrieve preset current at `index`.
     *
     * @param index One-based index, i.e. `1` refers to `M1`.
     * @return true On success.
     * @return false On failure.
     */
    bool get_preset_current_out(const uint8_t index, double &current);

    /**
     * @brief Store preset OVP at `index`.
     *
     * @param index One-based index, i.e. `1` refers to `M1`.
     * @return true On success.
     * @return false On failure.
     */
    bool set_preset_over_voltage_protection(const uint8_t index, const double voltage);

    /**
     * @brief Retrieve preset OVP at `index`.
     *
     * @param index One-based index, i.e. `1` refers to `M1`.
     * @return true On success.
     * @return false On failure.
     */
    bool get_preset_over_voltage_protection(const uint8_t index, double &voltage);

    /**
     * @brief Store preset OCP at `index`.
     *
     * @param index One-based index, i.e. `1` refers to `M1`.
     * @return true On success.
     * @return false On failure.
     */
    bool set_preset_over_current_protection(const uint8_t index, const double current);

    /**
     * @brief Retrieve preset OCP at `index`.
     *
     * @param index One-based index, i.e. `1` refers to `M1`.
     * @return true On success.
     * @return false On failure.
     */
    bool get_preset_over_current_protection(const uint8_t index, double &current);

    // Bootloader
    bool reboot_to_bootloader();

    // Shortcuts
    bool set_over_voltage_protection(const double voltage); // = M0_OVP
    bool set_over_current_protection(const double current); // = M0_OCP

    // Raw Access
    bool read_holding_registers(const uint16_t offset, uint16_t *value, const uint16_t numregs = 1);
    bool write_holding_register(const uint16_t offset, const uint16_t value);
    bool write_holding_registers(const uint16_t offset, uint16_t *value, uint16_t numregs = 1);
    bool read_holding_registers(const Register reg, uint16_t *value, const uint16_t numregs = 1);
    bool write_holding_register(const Register reg, const uint16_t value);
    bool write_holding_registers(const Register reg, uint16_t *value, uint16_t numregs = 1);

  private:
    ModbusRTU modbus;
    unsigned long timeout = 500; // milliseconds
    bool initialized = false;
    String type;

    double v_multi = 100.0;
    double i_multi = 100.0;
    double p_multi = 100.0;
    double v_in_multi = 100.0;

    /**
     *  Wait until no transaction is active or timeout.
     *
     * @return `false` on failure.
     */
    bool wait_for_inactive();

    bool read_voltage(const Register reg, double &voltage);
    bool write_voltage(const Register reg, double voltage);
    bool read_current(const Register reg, double &current);
    bool write_current(const Register reg, double current);
    bool read_power(const Register reg, double &power);
    bool read_boolean(const Register reg, boolean &b);
    bool write_boolean(const Register reg, boolean b);

    double value_to_voltage(const uint16_t value);
    double value_to_voltage_in(const uint16_t value);
    double value_to_current(const uint16_t value);
    double value_to_power(const uint16_t value);
    uint16_t voltage_to_value(const double voltage);
    uint16_t current_to_value(const double current);
    double values_to_temperature(const uint16_t *values);
    double values_to_ah(const uint16_t *values);
    double values_to_wh(const uint16_t *values);
    Protection value_to_protection(const uint16_t value);
    OutputMode value_to_output_mode(const uint16_t value);
    void values_to_tm(tm &time, const uint16_t *values);
    void tm_to_values(uint16_t *values, const tm &time);
    void values_to_preset(Preset &preset, const uint16_t *values);
    void preset_to_values(uint16_t *values, const Preset &preset);
};

} // namespace RidenDongle
