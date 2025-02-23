// SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
//
// SPDX-License-Identifier: MIT

#include <riden_config/riden_config.h>
#include <riden_http_server/riden_http_server.h>
#include <riden_logging/riden_logging.h>
#include <riden_modbus/riden_modbus.h>
#include <riden_modbus_bridge/riden_modbus_bridge.h>
#include <riden_scpi/riden_scpi.h>
#include <vxi11_server/rpc_bind_server.h>
#include <vxi11_server/vxi_server.h>
#include <scpi_bridge/scpi_bridge.h>

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiGratuitous.h>
#include <ESP8266mDNS.h>
#include <Ticker.h>
#include <WiFiManager.h>
#include <coredecls.h>
#include <time.h>

#define NTP_SERVER "pool.ntp.org"

#ifdef MOCK_RIDEN
#define MODBUS_USE_SOFWARE_SERIAL
#endif

using namespace RidenDongle;

static Ticker led_ticker;
static char hostname[100];

static bool has_time = false;
static bool did_update_time = false;

static bool connected = false;

static RidenModbus riden_modbus;                      ///< The modbus server
static RidenScpi riden_scpi(riden_modbus);            ///< The raw socket server + the SCPI command handler
static RidenModbusBridge modbus_bridge(riden_modbus); ///< The modbus TCP server
static SCPI_handler scpi_handler(riden_scpi);         ///< The bridge from the vxi server to the SCPI command handler
static VXI_Server vxi_server(scpi_handler);           ///< The vxi server
static RPC_Bind_Server rpc_bind_server(vxi_server);   ///< The RPC_Bind_Server for the vxi server
static RidenHttpServer http_server(riden_modbus, riden_scpi, modbus_bridge, vxi_server); ///< The web server

/**
 * Invoked by led_ticker to flash the LED.
 */
static void tick();

/**
 *  Gets called when WiFiManager enters configuration mode.
 */
static void wifi_manager_config_mode_callback(WiFiManager *myWiFiManager);

/**
 * Connect to WiFi.
 *
 * Start WiFiManager access point if no WiFi credentials are found
 * or the WiFi module fails to connect.
 *
 * @param hostname Name to advertise as hostname and WiFiManager access point.
 */
static bool connect_wifi(const char *hostname);

/**
 * Invoked when time has been received from an NTP server.
 */
static void on_time_received();

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    led_ticker.attach(0.6, tick);

#ifdef MODBUS_USE_SOFWARE_SERIAL
    Serial.begin(74880);
    delay(1000);
#endif

    riden_config.begin();

    // Wait for power supply firmware to boot
    unsigned long boot_delay_start = millis();
    while(!riden_modbus.begin()){
        if(millis() - boot_delay_start >= 5000L) break;
        delay(100);
    }

    // We need modbus initialised to read type and serial number
    if (riden_modbus.is_connected()) {
        uint32_t serial_number;
        riden_modbus.get_serial_number(serial_number);
        sprintf(hostname, "%s-%08u", riden_modbus.get_type().c_str(), serial_number);

        if (!connect_wifi(hostname)) {
            ESP.reset();
            delay(1000);
        }

        riden_scpi.begin();
        modbus_bridge.begin();
        vxi_server.begin();
        rpc_bind_server.begin();

        // turn off led
        led_ticker.detach();
        digitalWrite(LED_BUILTIN, HIGH);

        connected = true;
    } else {
        if (!connect_wifi(nullptr)) {
            ESP.reset();
            delay(1000);
        }
        led_ticker.attach(0.1, tick);
        connected = false;
    }

    http_server.begin();
}

static bool connect_wifi(const char *hostname)
{
    LOG_LN("WiFi initializing");

    WiFiManager wifiManager;
    wifiManager.setHostname(hostname);
    wifiManager.setDebugOutput(false);
    wifiManager.setAPCallback(wifi_manager_config_mode_callback);

    bool force_wifi_configuration = riden_config.get_and_reset_config_portal_on_boot();

    bool wifi_connected = false;
    if (force_wifi_configuration) {
        LOG_LN("WiFi starting configuration portal");
        wifi_connected = wifiManager.startConfigPortal(hostname);
    } else {
        LOG_LN("WiFi auto-connecting");
        wifi_connected = wifiManager.autoConnect(hostname);
    }
    if (wifi_connected) {

        LOG_F("WiFi SSID: %s\r\n", WiFi.SSID().c_str());
        LOG_F("IP: %s\r\n", WiFi.localIP().toString().c_str());

        experimental::ESP8266WiFiGratuitous::stationKeepAliveSetIntervalMs();
        if (hostname != nullptr) {
            if (!MDNS.begin(hostname)) {
                while (true) {
                    delay(100);
                }
            }
            String tz = riden_config.get_timezone_spec();
            if (tz.length() > 0) {
                // Get time via NTP
                settimeofday_cb(on_time_received);
                configTime(tz.c_str(), NTP_SERVER);
            }
        }
        ArduinoOTA.setHostname(hostname);
        ArduinoOTA.begin();
        MDNS.addServiceTxt("arduino", "tcp", "app_version", RidenDongle::version_string);
        if (RidenDongle::build_time != nullptr) {
            MDNS.addServiceTxt("arduino", "tcp", "build_date", RidenDongle::build_time);
        }
        MDNS.addServiceTxt("arduino", "tcp", "mac", WiFi.macAddress());
        LOG_LN("WiFi initialized");
    } else {
        LOG_LN("WiFi failed to initialize");
    }

    return wifi_connected;
}

void loop()
{
    if (connected) {
        if (has_time && !did_update_time) {
            LOG_LN("Setting PSU clock");
            // Read time and convert to local timezone
            time_t now;
            tm tm;
            time(&now);
            localtime_r(&now, &tm);

            riden_modbus.set_clock(tm);
            did_update_time = true;
        }

        MDNS.update();
        riden_modbus.loop();
        riden_scpi.loop();
        modbus_bridge.loop();
        rpc_bind_server.loop();
        vxi_server.loop();
    }
    http_server.loop();
    ArduinoOTA.handle();
}

void tick()
{
    // Toggle led state
    int state = digitalRead(LED_BUILTIN);
    digitalWrite(LED_BUILTIN, !state);
}

void wifi_manager_config_mode_callback(WiFiManager *myWiFiManager)
{
    // entered config mode, make led toggle faster
    led_ticker.attach(0.2, tick);
}

void on_time_received()
{
    LOG_LN("Time has been received");
    has_time = true;
}
