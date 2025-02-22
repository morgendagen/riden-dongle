// SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
//
// SPDX-License-Identifier: MIT

#pragma once

#include <riden_modbus/riden_modbus.h>

#include <ESP8266WiFi.h>
#include <SCPI_Parser.h>
#include <list>

#define WRITE_BUFFER_LENGTH (256)
#define SCPI_INPUT_BUFFER_LENGTH 256
#define SCPI_ERROR_QUEUE_SIZE 17
#define DEFAULT_SCPI_PORT 5025

namespace RidenDongle
{

class RidenScpi
{
  public:
    explicit RidenScpi(RidenModbus &ridenModbus, uint16_t port = DEFAULT_SCPI_PORT) : ridenModbus(ridenModbus), tcpServer(port) {}

    bool begin();
    bool loop();

    uint16_t port();
    std::list<IPAddress> get_connected_clients();
    void disconnect_client(const IPAddress &ip);

    const char *get_visa_resource();

  private:
    RidenModbus &ridenModbus;

    bool initialized = false;
    const char *idn1 = "Riden"; // <company name>
    char idn2[20] = {0};        // <model number>
    char idn3[10] = {0};        // <serial number>
    char idn4[10] = {0};        // <firmware revision>

    scpi_t scpi_context;
    char scpi_input_buffer[SCPI_INPUT_BUFFER_LENGTH] = {};
    scpi_error_t scpi_error_queue_data[SCPI_ERROR_QUEUE_SIZE];

    char write_buffer[WRITE_BUFFER_LENGTH] = {};
    size_t write_buffer_length = 0;

    static const scpi_command_t scpi_commands[];
    static scpi_interface_t scpi_interface;

    WiFiServer tcpServer;
    WiFiClient client;

    void reset_buffers();

    // SCPI Functions and Commands
    // ===========================
    // These are PascalCase in order to match SCPI Parser naming
    // conventions.
    static size_t SCPI_Write(scpi_t *context, const char *data, size_t len);
    static scpi_result_t SCPI_Flush(scpi_t *context);
    scpi_result_t SCPI_FlushRaw(void);
  
    static int SCPI_Error(scpi_t *context, int_fast16_t err);
    static scpi_result_t SCPI_Control(scpi_t *context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val);
    static scpi_result_t SCPI_Reset(scpi_t *context);

    static scpi_result_t Rcl(scpi_t *context);

    static scpi_result_t DisplayBrightness(scpi_t *context);
    static scpi_result_t DisplayBrightnessQ(scpi_t *context);
    static scpi_result_t DisplayLanguage(scpi_t *context);
    static scpi_result_t DisplayLanguageQ(scpi_t *context);

    static scpi_result_t SystemDate(scpi_t *context);
    static scpi_result_t SystemDateQ(scpi_t *context);
    static scpi_result_t SystemTime(scpi_t *context);
    static scpi_result_t SystemTimeQ(scpi_t *context);

    static scpi_result_t SourceVoltage(scpi_t *context);
    static scpi_result_t SourceVoltageQ(scpi_t *context);
    static scpi_result_t SourceVoltageProtectionTrippedQ(scpi_t *context);
    static scpi_result_t SourceCurrent(scpi_t *context);
    static scpi_result_t SourceCurrentQ(scpi_t *context);
    static scpi_result_t SourceCurrentProtectionTrippedQ(scpi_t *context);
    static scpi_result_t SourceVoltageLimit(scpi_t *context);
    static scpi_result_t SourceVoltageLimitQ(scpi_t *context);
    static scpi_result_t SourceCurrentLimit(scpi_t *context);
    static scpi_result_t SourceCurrentLimitQ(scpi_t *context);

    static scpi_result_t OutputState(scpi_t *context);
    static scpi_result_t OutputStateQ(scpi_t *context);
    static scpi_result_t OutputModeQ(scpi_t *context);

    static scpi_result_t MeasureVoltageQ(scpi_t *context);
    static scpi_result_t MeasureCurrentQ(scpi_t *context);
    static scpi_result_t MeasurePowerQ(scpi_t *context);
    static scpi_result_t MeasureTemperatureQ(scpi_t *context);

    static scpi_result_t SystemBeeperState(scpi_t *context);
    static scpi_result_t SystemBeeperStateQ(scpi_t *context);
};

} // namespace RidenDongle
