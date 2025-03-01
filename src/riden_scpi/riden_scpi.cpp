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

// ************
// This file combines a socket server with an SCPI parser.
//
// 3 main different types of socket servers are supported or are potentially possible:
// 
// ** RAW socket
// The default. Requires no special flags.
// scpi-raw uses a raw TCP connection to send and receive SCPI commands.
// This FW implementation supports only 1 client.
// - Discovery is done via mDNS, and the service name is "scpi-raw" (_scpi-raw._tcp).
// - The VISA string is like: "TCPIP::<ip address>::5025::SOCKET" (using the default port 5025)
// - The SCPI commands and responses are sent as plain text, delimited by newline characters.
// - is not discoverable by pyvisa, and requires a construction like this in Python:
// if args.port.endswith("::SOCKET"):
//     inst.read_termination = "\n"
//     inst.write_termination = "\n"
// ==> this is in this file
//
//
// ** VXI-11
// widely supported
// - Requires 2 socket services: portmap/rpcbind (port 111) and vxi-11 (any port you want)
// - Discovery is done via portmap when replying to GETPORT VXI-11 Core. This should be on UDP and TCP, but you can get by in TCP.
// - Secondary discovery is done via mDNS, and the service name is "vxi-11" (_vxi-11._tcp). That mapper points directly to the vxi server port.
// - The VISA string is like: "TCPIP::<ip address>::INSTR"
// - The SCPI commands and responses are sent as binary data, with a header and a payload.
// - VXI-11 has separate commands for reading and writing.
// - is discoverable by pyvisa, and requires no special construction in Python
// ==> this is in a parallel server, see vxi11_server rpc_bind_server
//
//
// ** HiSLIP 
// see https://www.ivifoundation.org/downloads/Protocol%20Specifications/IVI-6.1_HiSLIP-2.0-2020-04-23.pdf
// see https://lxistandard.org/members/Adopted%20Specifications/Latest%20Version%20of%20Standards_/LXI%20Version%201.6/LXI_HiSLIP_Extended_Function_1.3_2022-05-26.pdf
// is a more modern protocol. It can use a synchronous and/or an asynchronous connection. 
// - Discovery is done via mDNS, and the service name is "hislip" (_hislip._tcp)
// - The VISA string is like: "TCPIP::<ip address>::hislip0::INSTR" (using the default port 4880)
// - The SCPI commands and responses are sent as binary data, with a header and a payload.
// - It requires 2 connections on the same port (async and sync), even if you only use 1
// - is discoverable by pyvisa, and requires no special construction in Python other than installation of zeroconf
// ==> THE 2 CONNECTIONS MAKE IT NOT EASY TO DO WITHOUT REWRITING MUCH OF riden_scpi.cpp, AND WORK WAS ABANDONED


#ifdef MOCK_RIDEN
#define MODBUS_USE_SOFWARE_SERIAL
#endif

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
    LOG_F("SCPI_Write: writing \"%.*s\"\n", (int)len, data);
    ridenScpi->external_output_ready = false; // don't send half baked data to the client
    memcpy(&(ridenScpi->write_buffer[ridenScpi->write_buffer_length]), data, len);
    ridenScpi->write_buffer_length += len;
    
    return len;
}

scpi_result_t RidenScpi::SCPI_Flush(scpi_t *context)
{
    RidenScpi *ridenScpi = static_cast<RidenScpi *>(context->user_context);

    if (ridenScpi->external_control) {
        // do not write to the client, let the read function fetch the data
        ridenScpi->external_output_ready = true;
        return SCPI_RES_OK;
    }
    LOG_F("SCPI_Flush: sending \"%.*s\"\n", (int)ridenScpi->write_buffer_length, ridenScpi->write_buffer);
    if (ridenScpi->client) {
        ridenScpi->client.write(ridenScpi->write_buffer, ridenScpi->write_buffer_length);
        ridenScpi->write_buffer_length = 0;
        ridenScpi->client.flush();
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

    LOG_F("SourceVoltage command\n");

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

/**
 * @brief Write data to the parser and the device.
 * It overwrites the data in the buffer from the raw socket server.
 * 
 * @param data data to be sent
 * @param len length of data
 */
void RidenScpi::write(const char *data, size_t len) 
{
    if ((len == 0) || (data == NULL)) return;
    // insert the data into the buffer.
    if (len > SCPI_INPUT_BUFFER_LENGTH) {
        LOG_F("ERROR: RidenScpi buffer overflow. Ignoring data.\n");
        return;
    }
    memcpy(scpi_context.buffer.data, data, len);
    scpi_context.buffer.position = len;
    scpi_context.buffer.length = len;
    external_control = true; // just to be sure
    SCPI_Input(&scpi_context, NULL, 0);
}

/**
 * @brief Read data from the parser and the device, this is the reaction to "write()"
 * 
 * @param data buffer to copy the data into
 * @param len length of data
 * @param max_len maximum length of data
 * @return scpi_result_t last error code
 */
scpi_result_t RidenScpi::read(char *data, size_t *len, size_t max_len){
    if (!external_control || len == NULL || data == NULL) {
        return SCPI_RES_ERR;
    }
    *len = 0;
    if (write_buffer_length > max_len) {
        LOG_F("ERROR: RidenScpi output buffer overflow. Flushing the data.\n");
        return SCPI_RES_ERR;
    }
    if (!external_output_ready) {

        return SCPI_RES_ERR;
    }
    memcpy(data, write_buffer, write_buffer_length);
    *len = write_buffer_length;
    write_buffer_length = 0;
    return SCPI_RES_OK;
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

    if (MDNS.isRunning()) {
        LOG_LN("RidenScpi advertising as scpi-raw.");
        auto scpi_service = MDNS.addService(NULL, "scpi-raw", "tcp", tcpServer.port());
        MDNS.addServiceTxt(scpi_service, "version", SCPI_STD_VERSION_REVISION);
    }

    LOG_LN("RidenScpi initialized");

    initialized = true;
    return true;
}

bool RidenScpi::loop()
{
    if (external_control) {
        // skip this loop if I'm under external control
        if (client) {
            LOG_LN("RidenScpi: disconnect client because I am under external control.");
            client.stop();
        }
        return true;
    }

    // Check for new client connecting
    WiFiClient newClient = tcpServer.accept();
    if (newClient) {
        LOG_LN("RidenScpi: New client.");
        if (!client) {
            newClient.setTimeout(100);
            newClient.setNoDelay(true);
            client = newClient;
            reset_buffers();
        } else {
            newClient.stop();
        }
    }

    // Check for incoming data
    if (client) {
        int bytes_available = client.available();
        if (bytes_available > 0) {
            // Now read until I find a newline. There may be way more data in the buffer than 1 command.
            char buffer[1];
            while (client.readBytes(buffer, 1) == 1) {
                if (scpi_context.buffer.position >= SCPI_INPUT_BUFFER_LENGTH) {
                    // Client is sending more data than we can handle
                    LOG_F("ERROR: RidenScpi buffer overflow. Flushing data and killing connection.\n");
                    scpi_context.buffer.position = 0;
                    scpi_context.buffer.length = 0;
                    client.stop();
                    break;
                }
                // insert the character into the buffer.
                scpi_context.buffer.data[scpi_context.buffer.position] = buffer[0];
                scpi_context.buffer.position++;
                scpi_context.buffer.length++;
                if (buffer[0] == '\n') {
                    LOG_F("RidenScpi: received %d bytes for handling\n", scpi_context.buffer.position);
                    SCPI_Input(&scpi_context, NULL, 0);
                    break;
                }
            }
        }
    }

    // Stop client which disconnects
    if (client && !client.connected()) {
        LOG_LN("RidenScpi: disconnect client.");
        client.stop();
    }

    return true;
}

uint16_t RidenScpi::port()
{
    return tcpServer.port();
}

std::list<IPAddress> RidenScpi::get_connected_clients()
{
    std::list<IPAddress> connected_clients;
    if (client && client.connected()) {
        connected_clients.push_back(client.remoteIP());
    }
    return connected_clients;
}

void RidenScpi::disconnect_client(const IPAddress &ip)
{
    if (client && client.connected() && client.remoteIP() == ip) {
        client.stop();
    }
}

void RidenScpi::reset_buffers()
{
    write_buffer_length = 0;
    scpi_context.buffer.length = 0;
    scpi_context.buffer.position = 0;
}

const char *RidenScpi::get_visa_resource()
{
    static char visa_resource[40];
    sprintf(visa_resource, "TCPIP::%s::%u::SOCKET", WiFi.localIP().toString().c_str(), port());
    return visa_resource;
}


