// SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
//
// SPDX-License-Identifier: MIT

#include <riden_logging/riden_logging.h>
#include <riden_modbus/riden_modbus.h>
#include <riden_scpi/riden_scpi.h>

#include <Arduino.h>
#include <ESP8266mDNS.h>
#include <SCPI_Parser.h>
#include <functional>

using namespace RidenDongle;

// We only support one client

const scpi_command_t RidenScpi::scpi_commands[] = {
    /* IEEE Mandated Commands (SCPI std V1999.0 4.1.1) */
    {"*CLS", SCPI_CoreCls, 0},
    {"*ESE", SCPI_CoreEse, 0},
    {"*ESE?", SCPI_CoreEseQ, 0},
    {"*ESR?", SCPI_CoreEsrQ, 0},
    {"*IDN?", SCPI_CoreIdnQ, 0},
    {"*OPC", SCPI_CoreOpc, 0},
    {"*OPC?", SCPI_CoreOpcQ, 0},
    {"*RST", SCPI_CoreRst, 0},
    {"*SRE", SCPI_CoreSre, 0},
    {"*SRE?", SCPI_CoreSreQ, 0},
    {"*STB?", SCPI_CoreStbQ, 0},
    {"*TST?", SCPI_CoreTstQ, 0},
    {"*WAI", SCPI_CoreWai, 0},

    /* Required SCPI commands (SCPI std V1999.0 4.2.1) */
    {"SYSTem:ERRor[:NEXT]?", SCPI_SystemErrorNextQ, 0},
    {"SYSTem:ERRor:COUNt?", SCPI_SystemErrorCountQ, 0},
    {"SYSTem:VERSion?", SCPI_SystemVersionQ, 0},

    {"STATus:OPERation?", SCPI_StatusOperationEventQ, 0},
    {"STATus:OPERation:EVENt?", SCPI_StatusOperationEventQ, 0},
    {"STATus:OPERation:CONDition?", SCPI_StatusOperationConditionQ, 0},
    {"STATus:OPERation:ENABle", SCPI_StatusOperationEnable, 0},
    {"STATus:OPERation:ENABle?", SCPI_StatusOperationEnableQ, 0},

    {"STATus:QUEStionable[:EVENt]?", SCPI_StatusQuestionableEventQ, 0},
    {"STATus:QUEStionable:CONDition?", SCPI_StatusQuestionableConditionQ, 0},
    {"STATus:QUEStionable:ENABle", SCPI_StatusQuestionableEnable, 0},
    {"STATus:QUEStionable:ENABle?", SCPI_StatusQuestionableEnableQ, 0},

    {"STATus:PRESet", SCPI_StatusPreset, 0},

    {"*RCL", RidenScpi::Rcl, 0},
    {"DISPlay:BRIGhtness", RidenScpi::DisplayBrightness, 0},
    {"DISPlay:BRIGhtness?", RidenScpi::DisplayBrightnessQ, 0},
    {"DISPlay:LANGuage", RidenScpi::DisplayLanguage, 0},
    {"DISPlay:LANGuage?", RidenScpi::DisplayLanguageQ, 0},

    {"SYSTem:DATE", RidenScpi::SystemDate, 0},
    {"SYSTem:DATE?", RidenScpi::SystemDateQ, 0},
    {"SYSTem:TIME", RidenScpi::SystemTime, 0},
    {"SYSTem:TIME?", RidenScpi::SystemTimeQ, 0},

    {"OUTPut[:STATe]", RidenScpi::OutputState, 0},
    {"OUTPut[:STATe]?", RidenScpi::OutputStateQ, 0},
    {"OUTPut:MODE?", RidenScpi::OutputModeQ, 0},

    {"[SOURce]:VOLTage[:LEVel][:IMMediate][:AMPLitude]", RidenScpi::SourceVoltage, 0},
    {"[SOURce]:VOLTage[:LEVel][:IMMediate][:AMPLitude]?", RidenScpi::SourceVoltageQ, 0},
    {"[SOURce]:VOLTage:PROTection:TRIPped?", RidenScpi::SourceVoltageProtectionTrippedQ, 0},

    {"[SOURce]:CURRent[:LEVel][:IMMediate][:AMPLitude]", RidenScpi::SourceCurrent, 0},
    {"[SOURce]:CURRent[:LEVel][:IMMediate][:AMPLitude]?", RidenScpi::SourceCurrentQ, 0},
    {"[SOURce]:CURRent:PROTection:TRIPped?", RidenScpi::SourceCurrentProtectionTrippedQ},

    {"MEASure[:SCALar]:VOLTage[:DC]?", RidenScpi::MeasureVoltageQ, 0},
    {"MEASure[:SCALar]:CURRent[:DC]?", RidenScpi::MeasureCurrentQ, 0},
    {"MEASure[:SCALar]:POWer[:DC]?", RidenScpi::MeasurePowerQ, 0},
    {"MEASure[:SCALar]:TEMPerature[:THERmistor][:DC]?", RidenScpi::MeasureTemperatureQ, 0},

    {"[SOURce]:VOLTage:LIMit", RidenScpi::SourceVoltageLimit, 0},

    {"[SOURce]:CURRent:LIMit", RidenScpi::SourceCurrentLimit, 0},

    {"SYSTem:BEEPer:STATe", RidenScpi::SystemBeeperState, 0},
    {"SYSTem:BEEPer:STATe?", RidenScpi::SystemBeeperStateQ, 0},

    SCPI_CMD_LIST_END};

scpi_choice_def_t temperature_options[] = {
    {.name = "SYSTEM", .tag = 0},
    {.name = "PROBE", .tag = 1},
    SCPI_CHOICE_LIST_END,
};

scpi_choice_def_t language_options[] = {
    {.name = "ENGLISH", .tag = 0},
    {.name = "CHINESE", .tag = 1},
    {.name = "GERMAN", .tag = 2},
    {.name = "FRENCH", .tag = 3},
    {.name = "RUSSIAN", .tag = 4},
    SCPI_CHOICE_LIST_END,
};

scpi_interface_t RidenScpi::scpi_interface = {
    .error = RidenScpi::SCPI_Error,
    .write = RidenScpi::SCPI_Write,
    .control = RidenScpi::SCPI_Control,
    .flush = RidenScpi::SCPI_Flush,
    .reset = RidenScpi::SCPI_Reset,
};

size_t SCPI_ResultChoice(scpi_t *context, scpi_choice_def_t *options, int32_t value)
{
    for (int i = 0; options[i].name; ++i) {
        if (options[i].tag == value) {
            return SCPI_ResultMnemonic(context, options[i].name);
        }
    }
    return SCPI_ResultInt32(context, value);
}

size_t RidenScpi::SCPI_Write(scpi_t *context, const char *data, size_t len)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    memcpy(&(ridenScpi->write_buffer[ridenScpi->write_buffer_length]), data, len);
    ridenScpi->write_buffer_length += len;

    return len;
}

scpi_result_t RidenScpi::SCPI_Flush(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    if (ridenScpi->clients[0]) {
        ridenScpi->clients[0].write(ridenScpi->write_buffer, ridenScpi->write_buffer_length);
        ridenScpi->write_buffer_length = 0;
        ridenScpi->clients[0].flush();
    }

    return SCPI_RES_OK;
}

int RidenScpi::SCPI_Error(scpi_t *context, int_fast16_t err)
{
    (void)context;
    LOG_F(" * *ERROR : % d, \"%s\"\r\n", err, SCPI_ErrorTranslate(err));
    return 0;
}

scpi_result_t RidenScpi::SCPI_Control(scpi_t *context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val)
{
    LOG_LN("SCPI_Control");
    (void)context;
#ifdef MODBUS_USE_SOFWARE_SERIAL
    if (SCPI_CTRL_SRQ == ctrl) {
        Serial.print("**SRQ: 0x");
        Serial.print(val, HEX);
        Serial.print("(");
        Serial.print(val, DEC);
        Serial.println(")");
    } else {
        Serial.print("**CTRL: ");
        Serial.print(val, HEX);
        Serial.print("(");
        Serial.print(val, DEC);
        Serial.println(")");
    }
#endif
    return SCPI_RES_OK;
};

scpi_result_t RidenScpi::SCPI_Reset(scpi_t *context)
{
    (void)context;
    LOG_LN("**Reset");
    return SCPI_RES_OK;
}

scpi_result_t RidenScpi::Rcl(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    uint32_t profile;
    if (!SCPI_ParamUnsignedInt(context, &profile, true)) {
        return SCPI_RES_ERR;
    }
    if (profile < 1 || profile - 1 >= NUMBER_OF_PRESETS) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }
    if (ridenScpi->ridenModbus.set_preset(profile)) {
        return SCPI_RES_OK;
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
        return SCPI_RES_ERR;
    }
}

scpi_result_t RidenScpi::DisplayBrightness(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    uint32_t brightness;
    if (!SCPI_ParamUnsignedInt(context, &brightness, true)) {
        return SCPI_RES_ERR;
    }
    if (brightness > 5) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }
    if (ridenScpi->ridenModbus.set_brightness(brightness)) {
        return SCPI_RES_OK;
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
        return SCPI_RES_ERR;
    }
}

scpi_result_t RidenScpi::DisplayBrightnessQ(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    uint8_t brightness;
    if (ridenScpi->ridenModbus.get_brightness(brightness)) {
        SCPI_ResultUInt8(context, brightness);
        return SCPI_RES_OK;
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
        return SCPI_RES_ERR;
    }
}

scpi_result_t RidenScpi::DisplayLanguage(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    int32_t language = -1;
    scpi_parameter_t param;

    if (!SCPI_Parameter(context, &param, true)) {
        return SCPI_RES_ERR;
    }
    if (!SCPI_ParamToChoice(context, &param, language_options, &language)) {
        if (!SCPI_ParamToInt(context, &param, &language)) {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }
    }
    if (language < 0 || language > 4) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }
    if (ridenScpi->ridenModbus.set_language(language)) {
        return SCPI_RES_OK;
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
        return SCPI_RES_ERR;
    }
}

scpi_result_t RidenScpi::DisplayLanguageQ(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    uint16_t language;
    if (ridenScpi->ridenModbus.get_language(language)) {
        SCPI_ResultChoice(context, language_options, language);
        return SCPI_RES_OK;
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
        return SCPI_RES_ERR;
    }
}

scpi_result_t RidenScpi::SystemDate(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    uint32_t year, month, day;
    if (!SCPI_ParamUnsignedInt(context, &year, true) || !SCPI_ParamUnsignedInt(context, &month, true) || !SCPI_ParamUnsignedInt(context, &day, true)) {
        return SCPI_RES_ERR;
    }
    if (ridenScpi->ridenModbus.set_date(year, month, day)) {
        return SCPI_RES_OK;
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
        return SCPI_RES_ERR;
    }
}

scpi_result_t RidenScpi::SystemDateQ(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    tm clock;
    if (ridenScpi->ridenModbus.get_clock(clock)) {
        SCPI_ResultUInt16(context, clock.tm_year + 1900);
        SCPI_ResultUInt16(context, clock.tm_mon + 1);
        SCPI_ResultUInt16(context, clock.tm_mday);
        LOG_LN("SystemDateQ");
        return SCPI_RES_OK;
    } else {
        LOG_LN("SystemDateQ failure");
        SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
        return SCPI_RES_ERR;
    }
}

scpi_result_t RidenScpi::SystemTime(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    uint32_t hour, minute, second;
    if (!SCPI_ParamUnsignedInt(context, &hour, true) || !SCPI_ParamUnsignedInt(context, &minute, true) || !SCPI_ParamUnsignedInt(context, &second, true)) {
        return SCPI_RES_ERR;
    }
    if (ridenScpi->ridenModbus.set_time(hour, minute, second)) {
        return SCPI_RES_OK;
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
        return SCPI_RES_ERR;
    }
}

scpi_result_t RidenScpi::SystemTimeQ(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    tm clock;
    if (ridenScpi->ridenModbus.get_clock(clock)) {
        SCPI_ResultUInt16(context, clock.tm_hour);
        SCPI_ResultUInt16(context, clock.tm_min);
        SCPI_ResultUInt16(context, clock.tm_sec);
        return SCPI_RES_OK;
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
        return SCPI_RES_ERR;
    }
}

scpi_result_t RidenScpi::OutputState(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    bool on;
    if (!SCPI_ParamBool(context, &on, true)) {
        return SCPI_RES_ERR;
    }
    if (ridenScpi->ridenModbus.set_output_on(on)) {
        return SCPI_RES_OK;
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
        return SCPI_RES_ERR;
    }
}

scpi_result_t RidenScpi::OutputStateQ(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    bool on;
    if (ridenScpi->ridenModbus.get_output_on(on)) {
        SCPI_ResultBool(context, on);
        return SCPI_RES_OK;
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
        return SCPI_RES_ERR;
    }
}

scpi_result_t RidenScpi::OutputModeQ(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    OutputMode output_mode;
    if (ridenScpi->ridenModbus.get_output_mode(output_mode)) {
        switch (output_mode) {
        case OutputMode::CONSTANT_VOLTAGE:
            SCPI_ResultText(context, "CV");
            return SCPI_RES_OK;
        case OutputMode::CONSTANT_CURRENT:
            SCPI_ResultText(context, "CC");
            return SCPI_RES_OK;
        default:
            SCPI_ResultText(context, "XX");
            return SCPI_RES_OK;
        }
    }
    return SCPI_RES_ERR;
}

scpi_result_t RidenScpi::SourceVoltage(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    scpi_choice_def_t special;
    scpi_number_t value;

    if (!SCPI_ParamNumber(context, &special, &value, TRUE)) {
        return SCPI_RES_ERR;
    }
    if (value.unit != SCPI_UNIT_NONE && value.unit != SCPI_UNIT_VOLT) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_TYPE_ERROR);
        return SCPI_RES_ERR;
    }
    if (ridenScpi->ridenModbus.set_voltage_set(value.content.value)) {
        return SCPI_RES_OK;
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
        return SCPI_RES_ERR;
    }
}

scpi_result_t RidenScpi::SourceVoltageQ(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    double voltage;

    if (ridenScpi->ridenModbus.get_voltage_set(voltage)) {
        SCPI_ResultDouble(context, voltage);
        return SCPI_RES_OK;
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
        return SCPI_RES_ERR;
    }
}

scpi_result_t RidenScpi::SourceVoltageProtectionTrippedQ(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    Protection protection;
    if (ridenScpi->ridenModbus.get_protection(protection)) {
        SCPI_ResultBool(context, protection == Protection::OVP);
        return SCPI_RES_OK;
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
        return SCPI_RES_ERR;
    }
}

scpi_result_t RidenScpi::SourceCurrent(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    scpi_choice_def_t special;
    scpi_number_t value;

    if (!SCPI_ParamNumber(context, &special, &value, TRUE)) {
        return SCPI_RES_ERR;
    }
    if (value.unit != SCPI_UNIT_NONE && value.unit != SCPI_UNIT_AMPER) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_TYPE_ERROR);
        return SCPI_RES_ERR;
    }
    if (ridenScpi->ridenModbus.set_current_set(value.content.value)) {
        return SCPI_RES_OK;
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
        return SCPI_RES_ERR;
    }
}

scpi_result_t RidenScpi::SourceCurrentQ(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    double current;

    if (ridenScpi->ridenModbus.get_current_set(current)) {
        SCPI_ResultDouble(context, current);
        return SCPI_RES_OK;
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
        return SCPI_RES_ERR;
    }
}

scpi_result_t RidenScpi::SourceCurrentProtectionTrippedQ(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    Protection protection;
    if (ridenScpi->ridenModbus.get_protection(protection)) {
        SCPI_ResultBool(context, protection == Protection::OCP);
        return SCPI_RES_OK;
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
        return SCPI_RES_ERR;
    }
}

scpi_result_t RidenScpi::MeasureVoltageQ(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    double voltage;

    if (ridenScpi->ridenModbus.get_voltage_out(voltage)) {
        SCPI_ResultDouble(context, voltage);
        return SCPI_RES_OK;
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
        return SCPI_RES_ERR;
    }
}

scpi_result_t RidenScpi::MeasureCurrentQ(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    double current;

    if (ridenScpi->ridenModbus.get_current_out(current)) {
        SCPI_ResultDouble(context, current);
        return SCPI_RES_OK;
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
        return SCPI_RES_ERR;
    }
}

scpi_result_t RidenScpi::MeasurePowerQ(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    double power;

    if (ridenScpi->ridenModbus.get_power_out(power)) {
        SCPI_ResultDouble(context, power);
        return SCPI_RES_OK;
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
        return SCPI_RES_ERR;
    }
}

scpi_result_t RidenScpi::MeasureTemperatureQ(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    int32_t choice;
    if (!SCPI_ParamChoice(context, temperature_options, &choice, TRUE)) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }
    double temperature;
    bool success;
    if (choice == 0) {
        success = ridenScpi->ridenModbus.get_system_temperature_celsius(temperature);
    } else {
        success = ridenScpi->ridenModbus.get_probe_temperature_celsius(temperature);
    }
    if (success) {
        SCPI_ResultDouble(context, temperature);
        return SCPI_RES_OK;
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
        return SCPI_RES_ERR;
    }
}

scpi_result_t RidenScpi::SourceVoltageLimit(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    scpi_choice_def_t special;
    scpi_number_t value;

    if (!SCPI_ParamNumber(context, &special, &value, TRUE)) {
        return SCPI_RES_ERR;
    }
    if (value.unit != SCPI_UNIT_NONE && value.unit != SCPI_UNIT_VOLT) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_TYPE_ERROR);
        return SCPI_RES_ERR;
    }
    if (ridenScpi->ridenModbus.set_over_voltage_protection(value.content.value)) {
        return SCPI_RES_OK;
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
        return SCPI_RES_ERR;
    }
}

scpi_result_t RidenScpi::SourceCurrentLimit(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    scpi_choice_def_t special;
    scpi_number_t value;

    if (!SCPI_ParamNumber(context, &special, &value, TRUE)) {
        return SCPI_RES_ERR;
    }
    if (value.unit != SCPI_UNIT_NONE && value.unit != SCPI_UNIT_AMPER) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_TYPE_ERROR);
        return SCPI_RES_ERR;
    }
    if (ridenScpi->ridenModbus.set_over_current_protection(value.content.value)) {
        return SCPI_RES_OK;
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
        return SCPI_RES_ERR;
    }
}

scpi_result_t RidenScpi::SystemBeeperState(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    bool on;
    if (!SCPI_ParamBool(context, &on, TRUE)) {
        return SCPI_RES_ERR;
    }
    if (ridenScpi->ridenModbus.set_buzzer_enabled(on)) {
        return SCPI_RES_OK;
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
        return SCPI_RES_ERR;
    }
}

scpi_result_t RidenScpi::SystemBeeperStateQ(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    bool on;
    if (ridenScpi->ridenModbus.is_buzzer_enabled(on)) {
        SCPI_ResultBool(context, on);
        return SCPI_RES_OK;
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
        return SCPI_RES_ERR;
    }
}

bool RidenScpi::begin()
{
    if (initialized) {
        return true;
    }

    LOG_LN("RidenScpi initializing");

    String type = ridenModbus.get_type();
    uint32_t serial_number;
    ridenModbus.get_serial_number(serial_number);
    uint16_t firmware_version;
    ridenModbus.get_firmware_version(firmware_version);
    memcpy(idn2, type.c_str(), type.length());
    sprintf(idn3, "%08u", serial_number);
    sprintf(idn4, "%u.%u", firmware_version / 100u, firmware_version % 100u);

    const char *idn1 = "Riden"; // <company name>
    SCPI_Init(&scpi_context,
              scpi_commands,
              &scpi_interface,
              scpi_units_def,
              idn1, idn2, idn3, idn4,
              scpi_input_buffer, SCPI_INPUT_BUFFER_LENGTH,
              scpi_error_queue_data, SCPI_ERROR_QUEUE_SIZE);
    scpi_context.user_context = this;

    // Start TCP listener
    tcpServer.begin();
    tcpServer.setNoDelay(true);

    // Add scpi-raw service to MDNS-SD
    MDNS.addService("scpi-raw", "tcp", tcpServer.port());

    LOG_LN("RidenScpi initialized");

    initialized = true;
    return true;
}

bool RidenScpi::loop()
{
    // check for any new client connecting, and say hello (before any incoming data)
    WiFiClient newClient = tcpServer.accept();
    if (newClient) {
        bool reject = true;
        for (byte i = 0; i < SCPI_MAX_CLIENTS; i++) {
            if (!clients[i]) {
                // Once we "accept", the client is no longer tracked by WiFiServer
                // so we must store it into our list of clients
                newClient.setTimeout(100);
                newClient.setNoDelay(true);
                clients[i] = newClient;
                reject = false;
                break;
            }
        }
        if (reject) {
            newClient.stop();
        }
    }

    static char readBuffer[READ_BUFFER_LENGTH];

    // check for incoming data from all clients
    for (byte i = 0; i < SCPI_MAX_CLIENTS; i++) {
        if (clients[i].available() > 0) {
            size_t bytesRead = clients[i].readBytes(readBuffer, clients[i].available());
            SCPI_Input(&scpi_context, readBuffer, bytesRead);
        }
    }

    // stop any clients which disconnect
    for (byte i = 0; i < SCPI_MAX_CLIENTS; i++) {
        if (clients[i] && !clients[i].connected()) {
            clients[i].stop();
        }
    }

    return true;
}

uint16_t RidenScpi::port()
{
    return tcpServer.port();
}

std::list<String> RidenScpi::get_connected_clients()
{
    std::list<String> connected_clients;
    for (byte i = 0; i < SCPI_MAX_CLIENTS; i++) {
        if (clients[i] && clients[i].connected()) {
            LOG_LN(clients[i].remoteIP());
            connected_clients.push_back(clients[i].remoteIP().toString());
        }
    }
    return connected_clients;
}

void RidenScpi::disconnect_client(String ip)
{
    for (byte i = 0; i < SCPI_MAX_CLIENTS; i++) {
        if (clients[i] && clients[i].connected() && clients[i].remoteIP().toString() == ip) {
            clients[i].stop();
        }
    }
}
