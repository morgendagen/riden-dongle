// SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
//
// SPDX-License-Identifier: MIT

#include <riden_config/riden_config.h>
#include <riden_logging/riden_logging.h>
#include <riden_modbus/riden_modbus.h>

#include <Arduino.h>
#ifdef MODBUS_USE_SOFWARE_SERIAL
#include <SoftwareSerial.h>
#endif

#define BUF_SIZE 100

#ifdef MODBUS_USE_SOFWARE_SERIAL
SoftwareSerial SerialRuideng = SoftwareSerial(MODBUS_RX, MODBUS_TX);
#else
#define SerialRuideng Serial
#endif

using namespace RidenDongle;

bool RidenModbus::begin()
{
    if (initialized) {
        return true;
    }

    LOG_LN("RuidengModbus initializing");

#ifdef MODBUS_USE_SOFWARE_SERIAL
    SerialRuideng.begin(riden_config.get_uart_baudrate(), SWSERIAL_8N1);
#else
    SerialRuideng.begin(riden_config.get_uart_baudrate(), SERIAL_8N1);
#endif
    if (!modbus.begin(&SerialRuideng)) {
        LOG_LN("Failed initializing ModbusRTU");
        return false;
    }
    modbus.client();

    // we need to pretend we're connected
    // or else get_id() will fail.
    initialized = true;
    uint16_t id;
    if (!get_id(id)) {
        LOG_LN("Failed reading power supply id");
        initialized = false;
        return false;
    }
    initialized = false;

    if (60180 <= id && id <= 60189) {
        this->type = "RD6018";
    } else if (60120 <= id && id <= 60124) {
        this->type = "RD6012";
    } else if (60125 <= id && id <= 60129) {
        this->type = "RD6012P";
        this->v_multi = 1000;
        this->p_multi = 1000;
        // i_multi is not constant!
    } else if (60060 <= id && id <= 60064) {
        this->type = "RD6006";
        this->i_multi = 1000;
    } else if (id == 60065) {
        this->type = "RD6006P";
        this->v_multi = 1000;
        this->i_multi = 10000;
        this->p_multi = 1000;
    } else if (id == 60301) {
        this->type = "RD6030";
    } else if (60241 <= id) {
        this->type = "RD6024";
    } else {
        LOG_LN("Failed decoding power supply id");
        return false;
    }

    LOG_LN("RuidengModbus initialized");
    initialized = true;
    return true;
}

bool RidenModbus::loop()
{
    if (!initialized) {
        return false;
    }

    modbus.task();
    return true;
}

bool RidenModbus::is_connected()
{
    return initialized;
}

String RidenModbus::get_type()
{
    return type;
}

bool RidenModbus::get_all_values(AllValues &all_values)
{
    // Reading all registers at once fails silently, so
    // we read 20 registers at a time instead.
    Register last_reg = Register::M9_OCP;
    int total_nof_regs = (+last_reg) + 1;
    uint16_t values[total_nof_regs];
    for (int first_reg_to_read = 0; first_reg_to_read < total_nof_regs; first_reg_to_read += 20) {
        int regs_to_read = min(20, total_nof_regs - first_reg_to_read);
        if (!read_holding_registers(first_reg_to_read, &(values[first_reg_to_read]), regs_to_read)) {
            return false;
        }
    }

    all_values.system_temperature_celsius = values_to_temperature(&(values[+Register::SystemTemperatureCelsius_Sign]));
    all_values.system_temperature_fahrenheit = values_to_temperature(&(values[+Register::SystemTemperatureFarhenheit_Sign]));
    all_values.voltage_set = value_to_voltage(values[+Register::VoltageSet]);
    all_values.current_set = value_to_current(values[+Register::CurrentSet]);
    all_values.voltage_out = value_to_voltage(values[+Register::VoltageOut]);
    all_values.current_out = value_to_current(values[+Register::CurrentOut]);
    all_values.power_out = value_to_power(values[+Register::PowerOut]);
    all_values.voltage_in = value_to_voltage_in(values[+Register::VoltageIn]);
    all_values.keypad_locked = values[+Register::Keypad] != 0;
    all_values.protection = value_to_protection(values[+Register::Protection]);
    all_values.output_mode = value_to_output_mode(values[+Register::OutputMode]);
    all_values.output_on = values[+Register::Output] != 0;
    all_values.current_range = values[+Register::CurrentRange];
    all_values.is_battery_mode = values[+Register::BatteryMode] != 0;
    all_values.voltage_battery = value_to_voltage(values[+Register::VoltageBattery]);
    all_values.probe_temperature_celsius = values_to_temperature(&(values[+Register::ProbeTemperatureCelsius_Sign]));
    all_values.probe_temperature_fahrenheit = values_to_temperature(&(values[+Register::ProbeTemperatureFarhenheit_Sign]));
    all_values.ah = values_to_ah(&(values[+Register::AH_H]));
    all_values.wh = values_to_wh(&(values[+Register::WH_H]));
    values_to_tm(all_values.clock, &(values[+Register::Year]));
    all_values.is_take_ok = values[+Register::TakeOk] != 0;
    all_values.is_take_out = values[+Register::TakeOut] != 0;
    all_values.is_power_on_boot = values[+Register::PowerOnBoot] != 0;
    all_values.is_buzzer_enabled = values[+Register::Buzzer] != 0;
    all_values.is_logo = values[+Register::Logo] != 0;
    all_values.language = values[+Register::Language];
    all_values.brightness = values[+Register::Brightness];
    // Calibration
    all_values.calibration.V_OUT_ZERO = values[+Register::V_OUT_ZERO];
    all_values.calibration.V_OUT_SCALE = values[+Register::V_OUT_SCALE];
    all_values.calibration.V_BACK_ZERO = values[+Register::V_BACK_ZERO];
    all_values.calibration.V_BACK_SCALE = values[+Register::V_BACK_SCALE];
    all_values.calibration.I_OUT_ZERO = values[+Register::I_OUT_ZERO];
    all_values.calibration.I_OUT_SCALE = values[+Register::I_OUT_SCALE];
    all_values.calibration.I_BACK_ZERO = values[+Register::I_BACK_ZERO];
    all_values.calibration.I_BACK_SCALE = values[+Register::I_BACK_SCALE];
    // Presets - M0 is ignored
    for (int index = 0; index < NUMBER_OF_PRESETS; index++) {
        values_to_preset(all_values.presets[index], &(values[+Register::M0_V + 4 * (index + 1)]));
    }

    return true;
}

bool RidenModbus::reboot_to_bootloader()
{
    return write_holding_register(256, 5633);
}

bool RidenModbus::get_id(uint16_t &id)
{
    return read_holding_registers(Register::Id, &id);
}

bool RidenModbus::get_serial_number(uint32_t &serial_number)
{
    uint16_t value[2];
    if (!read_holding_registers(Register::SerialNumber_High, value, 2)) {
        return false;
    }
    serial_number = (uint32_t(value[0]) << 16) + uint32_t(value[1]);
    return true;
}

bool RidenModbus::get_firmware_version(uint16_t &firmware_version)
{
    return read_holding_registers(Register::Firmware, &firmware_version);
}

bool RidenModbus::get_system_temperature_celsius(double &temperature)
{
    uint16_t values[2];
    if (!read_holding_registers(Register::SystemTemperatureCelsius_Sign, values, 2)) {
        return false;
    }
    temperature = values_to_temperature(values);
    return true;
}

bool RidenModbus::get_system_temperature_fahrenheit(double &temperature)
{
    uint16_t values[2];
    if (!read_holding_registers(Register::SystemTemperatureFarhenheit_Sign, values, 2)) {
        return false;
    }
    temperature = values_to_temperature(values);
    return true;
}

bool RidenModbus::get_voltage_set(double &voltage)
{
    return read_voltage(Register::VoltageSet, voltage);
}

bool RidenModbus::set_voltage_set(const double voltage)
{
    return write_voltage(Register::VoltageSet, voltage);
}

bool RidenModbus::get_current_set(double &current)
{
    return read_current(Register::CurrentSet, current);
}

bool RidenModbus::set_current_set(const double current)
{
    return write_current(Register::CurrentSet, current);
}

bool RidenModbus::get_voltage_out(double &voltage)
{
    return read_voltage(Register::VoltageOut, voltage);
}

bool RidenModbus::get_current_out(double &current)
{
    return read_current(Register::CurrentOut, current);
}

bool RidenModbus::get_power_out(double &power)
{
    return read_power(Register::PowerOut, power);
}

bool RidenModbus::is_keypad_locked(bool &keypad)
{
    return read_boolean(Register::Keypad, keypad);
}

bool RidenModbus::get_protection(Protection &protection)
{
    uint16_t value;
    if (!read_holding_registers(Register::Protection, &value)) {
        return false;
    }
    protection = value_to_protection(value);
    return true;
}

bool RidenModbus::get_output_mode(OutputMode &output_mode)
{
    uint16_t value;
    if (!read_holding_registers(Register::OutputMode, &value)) {
        return false;
    }
    output_mode = value_to_output_mode(value);
    return true;
}

bool RidenModbus::get_output_on(bool &result)
{
    return read_boolean(Register::Output, result);
}

bool RidenModbus::set_output_on(const bool on)
{
    return write_boolean(Register::Output, on);
}

bool RidenModbus::set_preset(const uint8_t index)
{
    if (index < 1 || index - 1 >= NUMBER_OF_PRESETS) {
        return false;
    }
    return write_holding_register(Register::Preset, index);
}

bool RidenModbus::get_current_range(uint16_t &current_range)
{
    // TODO[pdr] conversion
    return read_holding_registers(Register::CurrentRange, &current_range);
}

bool RidenModbus::is_battery_mode(bool &battery_mode)
{
    return read_boolean(Register::BatteryMode, battery_mode);
}

bool RidenModbus::get_voltage_battery(double &voltage_battery)
{
    return read_voltage(Register::VoltageBattery, voltage_battery);
}

bool RidenModbus::get_probe_temperature_celsius(double &temperature)
{
    uint16_t values[2];
    if (!read_holding_registers(Register::ProbeTemperatureCelsius_Sign, values, 2)) {
        return false;
    }
    temperature = values_to_temperature(values);
    return true;
}

bool RidenModbus::get_probe_temperature_fahrenheit(double &temperature)
{
    uint16_t values[2];
    if (!read_holding_registers(Register::ProbeTemperatureFarhenheit_Sign, values, 2)) {
        return false;
    }
    temperature = values_to_temperature(values);
    return true;
}

bool RidenModbus::get_ah(double &ah)
{
    uint16_t values[2];
    if (!read_holding_registers(Register::AH_H, values, 2)) {
        return false;
    }
    ah = values_to_ah(values);
    return true;
}

bool RidenModbus::get_wh(double &wh)
{
    uint16_t values[2];
    if (!read_holding_registers(Register::WH_H, values, 2)) {
        return false;
    }
    wh = values_to_wh(values);
    return true;
}

bool RidenModbus::get_clock(tm &time)
{
    uint16_t values[6];
    bool res = read_holding_registers(Register::Year, values, 6);
    if (!res) {
        return false;
    }
    values_to_tm(time, values);
    return true;
}

bool RidenModbus::set_clock(const tm time)
{
    uint16_t values[6];
    tm_to_values(values, time);
    return write_holding_registers(Register::Year, values, 6);
}

bool RidenModbus::set_date(const uint16_t year, const uint16_t month, const uint16_t day)
{
    uint16_t values[3] = {year, month, day};
    return write_holding_registers(Register::Year, values, 3);
}

bool RidenModbus::set_time(const uint8_t hour, const uint8_t minute, const uint8_t second)
{
    uint16_t values[3] = {hour, minute, second};
    return write_holding_registers(Register::Hour, values, 3);
}

bool RidenModbus::is_take_ok(bool &take_ok)
{
    return read_boolean(Register::TakeOk, take_ok);
}

bool RidenModbus::set_take_ok(const bool take_ok)
{
    return write_boolean(Register::TakeOk, take_ok);
}

bool RidenModbus::is_take_out(bool &take_out)
{
    return read_boolean(Register::TakeOut, take_out);
}

bool RidenModbus::set_take_out(const bool take_out)
{
    return write_boolean(Register::TakeOut, take_out);
}

bool RidenModbus::is_power_on_boot(bool &power_on_boot)
{
    return read_boolean(Register::PowerOnBoot, power_on_boot);
}

bool RidenModbus::set_power_on_boot(const bool power_on_boot)
{
    return write_boolean(Register::PowerOnBoot, power_on_boot);
}

bool RidenModbus::is_buzzer_enabled(bool &buzzer)
{
    return read_boolean(Register::Buzzer, buzzer);
}

bool RidenModbus::set_buzzer_enabled(const bool buzzer)
{
    return write_boolean(Register::Buzzer, buzzer);
}

bool RidenModbus::is_logo(bool &logo)
{
    return read_boolean(Register::Logo, logo);
}

bool RidenModbus::set_logo(const bool logo)
{
    return write_boolean(Register::Logo, logo);
}

bool RidenModbus::get_language(uint16_t &language)
{
    return read_holding_registers(Register::Language, &language);
}

bool RidenModbus::set_language(const uint16_t language)
{
    return write_holding_register(Register::Language, language);
}

bool RidenModbus::get_brightness(uint8_t &brightness)
{
    uint16_t value;
    if (!read_holding_registers(Register::Brightness, &value)) {
        return false;
    }
    brightness = value;
    return true;
}

bool RidenModbus::set_brightness(const uint8_t brightness)
{
    return write_holding_register(Register::Brightness, brightness);
}

// Calibration
bool RidenModbus::get_calibration(Calibration &calibration)
{
    uint16_t values[8];
    if (!read_holding_registers(Register::V_OUT_ZERO, values, 8)) {
        return false;
    }
    calibration.V_OUT_ZERO = values[0];
    calibration.V_OUT_SCALE = values[1];
    calibration.V_BACK_ZERO = values[2];
    calibration.V_BACK_SCALE = values[3];
    calibration.I_OUT_ZERO = values[4];
    calibration.I_OUT_SCALE = values[5];
    calibration.I_BACK_ZERO = values[6];
    calibration.I_BACK_SCALE = values[7];
    return true;
}

bool RidenModbus::set_calibration(const Calibration calibration)
{
    uint16_t values[8] = {
        calibration.V_OUT_ZERO,
        calibration.V_OUT_SCALE,
        calibration.V_BACK_ZERO,
        calibration.V_BACK_SCALE,
        calibration.I_OUT_ZERO,
        calibration.I_OUT_SCALE,
        calibration.I_BACK_ZERO,
        calibration.I_BACK_SCALE,
    };
    return write_holding_registers(Register::V_OUT_ZERO, values, 8);
}

// Presets

bool RidenModbus::set_preset(const uint8_t index, const Preset &preset)
{
    if (index >= NUMBER_OF_PRESETS) {
        return false;
    }
    uint16_t values[4];
    preset_to_values(values, preset);
    Register reg = Register(+Register::M0_V + 4 * index);
    return write_holding_registers(reg, values, 4);
}

bool RidenModbus::get_preset(const uint8_t index, Preset &preset)
{
    if (index >= NUMBER_OF_PRESETS) {
        return false;
    }
    Register reg = Register(+Register::M0_V + 4 * index);
    uint16_t values[4];
    if (!read_holding_registers(reg, values, 4)) {
        return false;
    }
    values_to_preset(preset, values);
    return true;
}

bool RidenModbus::set_preset_voltage_out(const uint8_t index, const double voltage)
{
    if (index >= NUMBER_OF_PRESETS) {
        return false;
    }
    const Register reg = Register(+Register::M0_V + 4 * index);
    return write_voltage(reg, voltage);
}

bool RidenModbus::get_preset_voltage_out(const uint8_t index, double &voltage)
{
    if (index >= NUMBER_OF_PRESETS) {
        return false;
    }
    const Register reg = Register(+Register::M0_V + 4 * index);
    return read_voltage(reg, voltage);
}

bool RidenModbus::set_preset_current_out(const uint8_t index, const double current)
{
    if (index >= NUMBER_OF_PRESETS) {
        return false;
    }
    const Register reg = Register(+Register::M0_I + 4 * index);
    return write_current(reg, current);
}

bool RidenModbus::get_preset_current_out(const uint8_t index, double &current)
{
    if (index >= NUMBER_OF_PRESETS) {
        return false;
    }
    const Register reg = Register(+Register::M0_I + 4 * index);
    return read_current(reg, current);
}

bool RidenModbus::set_preset_over_voltage_protection(const uint8_t index, const double voltage)
{
    if (index >= NUMBER_OF_PRESETS) {
        return false;
    }
    const Register reg = Register(+Register::M0_OVP + 4 * index);
    return write_voltage(reg, voltage);
}

bool RidenModbus::get_preset_over_voltage_protection(const uint8_t index, double &voltage)
{
    if (index >= NUMBER_OF_PRESETS) {
        return false;
    }
    const Register reg = Register(+Register::M0_OVP + 4 * index);
    return read_voltage(reg, voltage);
}

bool RidenModbus::set_preset_over_current_protection(const uint8_t index, const double current)
{
    if (index >= NUMBER_OF_PRESETS) {
        return false;
    }
    const Register reg = Register(+Register::M0_OCP + 4 * index);
    return write_current(reg, current);
}

bool RidenModbus::get_preset_over_current_protection(const uint8_t index, double &current)
{
    if (index >= NUMBER_OF_PRESETS) {
        return false;
    }
    const Register reg = Register(+Register::M0_OCP + 4 * index);
    return read_current(reg, current);
}

// Shortcuts

bool RidenModbus::set_over_voltage_protection(const double voltage)
{
    return set_preset_over_voltage_protection(0, voltage);
}

bool RidenModbus::set_over_current_protection(const double current)
{
    return set_preset_over_current_protection(0, current);
}

// Helpers

bool RidenModbus::read_voltage(const Register reg, double &voltage)
{
    uint16_t value;
    if (!read_holding_registers(reg, &value)) {
        return false;
    }
    voltage = value_to_voltage(value);
    return true;
}

bool RidenModbus::write_voltage(const Register reg, const double voltage)
{
    const uint16_t value = voltage_to_value(voltage);
    return write_holding_register(reg, value);
}

bool RidenModbus::read_current(const Register reg, double &current)
{
    uint16_t value;
    if (!read_holding_registers(reg, &value)) {
        return false;
    }
    current = value_to_current(value);
    return true;
}

bool RidenModbus::write_current(const Register reg, const double current)
{
    const uint16_t value = current_to_value(current);
    return write_holding_register(reg, value);
}

bool RidenModbus::read_power(const Register reg, double &power)
{
    uint16_t value;
    if (!read_holding_registers(reg, &value)) {
        return false;
    }
    power = value_to_power(value);
    return true;
}

bool RidenModbus::read_boolean(const Register reg, boolean &b)
{
    uint16_t value = 0;
    if (!read_holding_registers(reg, &value)) {
        return false;
    }
    b = (value != 0);
    return true;
}

bool RidenModbus::write_boolean(const Register reg, const boolean b)
{
    const uint16_t value = b ? 1 : 0;
    return write_holding_register(reg, value);
}

bool RidenModbus::wait_for_inactive()
{
    if (!initialized) {
        return false;
    }
    // Wait until no transaction is active or timeout has passed
    unsigned long started_at = millis();
    unsigned long wait_until = started_at + timeout;
    while (modbus.server()) {
        delay(1);
        modbus.task();
        if (millis() > wait_until) {
            LOG_LN("Timed out waiting for response from power supply module");
            return false;
        }
    }
    return true;
}

bool RidenModbus::read_holding_registers(const uint16_t offset, uint16_t *value, const uint16_t numregs)
{
    if (!wait_for_inactive()) {
        return false;
    }
    bool res = modbus.readHreg(MODBUS_ADDRESS, offset, value, numregs);
    if (!res) {
        return false;
    }
    // Wait until we receive an answer
    return wait_for_inactive();
}

bool RidenModbus::write_holding_register(const uint16_t offset, const uint16_t value)
{
    if (!wait_for_inactive()) {
        return false;
    }
    bool res = modbus.writeHreg(MODBUS_ADDRESS, offset, value);
    if (!res) {
        return false;
    }
    // Wait until we receive an answer
    return wait_for_inactive();
}

bool RidenModbus::write_holding_registers(const uint16_t offset, uint16_t *value, uint16_t numregs)
{
    if (!wait_for_inactive()) {
        return false;
    }
    bool res = modbus.writeHreg(MODBUS_ADDRESS, offset, value, numregs);
    if (!res) {
        return false;
    }
    // Wait until we receive an answer
    return wait_for_inactive();
}

bool RidenModbus::read_holding_registers(const Register reg, uint16_t *value, const uint16_t numregs)
{
    uint16_t offset = +reg;
    return read_holding_registers(offset, value, numregs);
}

bool RidenModbus::write_holding_register(const Register reg, const uint16_t value)
{
    uint16_t offset = +reg;
    return write_holding_register(offset, value);
}

bool RidenModbus::write_holding_registers(const Register reg, uint16_t *value, uint16_t numregs)
{
    uint16_t offset = +reg;
    return write_holding_registers(offset, value, numregs);
}

double RidenModbus::value_to_voltage(const uint16_t value)
{
    return double(value) / v_multi;
}

double RidenModbus::value_to_voltage_in(const uint16_t value)
{
    return double(value) / v_in_multi;
}

double RidenModbus::value_to_current(const uint16_t value)
{
    return double(value) / i_multi;
}

double RidenModbus::value_to_power(const uint16_t value)
{
    return double(value) / p_multi;
}

uint16_t RidenModbus::voltage_to_value(const double voltage)
{
    return uint16_t(voltage * v_multi);
}

uint16_t RidenModbus::current_to_value(const double current)
{
    return uint16_t(current * i_multi);
}

double RidenModbus::values_to_temperature(const uint16_t *values)
{
    return (values[0] == 0 ? 1 : -1) * double(values[1]);
}

double RidenModbus::values_to_ah(const uint16_t *values)
{
    uint32_t value = (values[0] << 16) + values[1];
    return double(value) / 1000.0;
}

double RidenModbus::values_to_wh(const uint16_t *values)
{
    uint32_t value = (values[0] << 16) + values[1];
    return double(value) / 1000.0;
}

Protection RidenModbus::value_to_protection(const uint16_t value)
{
    switch (value) {
    case 1:
        return Protection::OVP;
    case 2:
        return Protection::OCP;
    default:
        return Protection::None;
    }
}

OutputMode RidenModbus::value_to_output_mode(const uint16_t value)
{
    switch (value) {
    case 0:
        return OutputMode::CONSTANT_VOLTAGE;
    case 1:
        return OutputMode::CONSTANT_CURRENT;
    default:
        return OutputMode::Unknown;
    }
}

void RidenModbus::values_to_tm(tm &time, const uint16_t *values)
{
    time.tm_year = values[0] - 1900;
    time.tm_mon = values[1] - 1;
    time.tm_mday = values[2];
    time.tm_hour = values[3];
    time.tm_min = values[4];
    time.tm_sec = values[5];
}

void RidenModbus::tm_to_values(uint16_t *values, const tm &time)
{
    values[0] = uint16_t(time.tm_year + 1900);
    values[1] = uint16_t(time.tm_mon + 1);
    values[2] = uint16_t(time.tm_mday);
    values[3] = uint16_t(time.tm_hour);
    values[4] = uint16_t(time.tm_min);
    values[5] = uint16_t(time.tm_sec);
}

void RidenModbus::values_to_preset(Preset &preset, const uint16_t *values)
{
    preset.voltage = value_to_voltage(values[0]);
    preset.current = value_to_current(values[1]);
    preset.over_voltage_protection = value_to_voltage(values[2]);
    preset.over_current_protection = value_to_current(values[3]);
}

void RidenModbus::preset_to_values(uint16_t *values, const Preset &preset)
{
    values[0] = voltage_to_value(preset.voltage);
    values[1] = current_to_value(preset.current);
    values[2] = voltage_to_value(preset.over_voltage_protection);
    values[3] = current_to_value(preset.over_current_protection);
}
