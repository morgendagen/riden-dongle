// SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
//
// SPDX-License-Identifier: MIT

#include "http_static.h"
#include <riden_config/riden_config.h>
#include <riden_http_server/riden_http_server.h>
#include <riden_logging/riden_logging.h>
#include <vxi11_server/vxi_server.h>

#include <ESP8266mDNS.h>
#include <TinyTemplateEngineMemoryReader.h>
#include <list>

using namespace RidenDongle;

static const String scpi_protocol = "SCPI";
static const String modbustcp_protocol = "Modbus TCP";
static const String vxi11_protocol = "VXI-11";
static const std::list<uint32_t> uart_baudrates = {
    9600,
    19200,
    38400,
    57600,
    115200,
    230400,
    250000,
    460800,
    921600,
    1000000,
};

static String voltage_to_string(double voltage)
{
    if (voltage < 1) {
        return String(voltage * 1000, 0) + " mV";
    } else {
        return String(voltage, 3) + " V";
    }
}

static String current_to_string(double current)
{
    if (current < 1) {
        return String(current * 1000, 0) + " mA";
    } else {
        return String(current, 3) + " A";
    }
}

static String power_to_string(double power)
{
    if (power < 1) {
        return String(power * 1000, 0) + " mW";
    } else {
        return String(power, 3) + " W";
    }
}

static String protection_to_string(Protection protection)
{
    switch (protection) {
    case Protection::OVP:
        return "OVP";
    case Protection::OCP:
        return "OCP";
    default:
        return "None";
    }
}

static String outputmode_to_string(OutputMode output_mode)
{
    switch (output_mode) {
    case OutputMode::CONSTANT_VOLTAGE:
        return "Constant Voltage";
    case OutputMode::CONSTANT_CURRENT:
        return "Constant Current";
    default:
        return "Unknown";
    }
}

static String language_to_string(uint16_t language_id)
{
    String language;
    switch (language_id) {
    case 0:
        language = "English";
        break;
    case 1:
        language = "Chinese";
        break;
    case 2:
        language = "German";
        break;
    case 3:
        language = "French";
        break;
    case 4:
        language = "Russian";
        break;
    default:
        language = "Unknown (" + String(language_id, 10) + ")";
        break;
    }
    return language;
}

bool RidenHttpServer::begin()
{
    server.on("/", HTTPMethod::HTTP_GET, std::bind(&RidenHttpServer::handle_root_get, this));
    server.on("/psu/", HTTP_GET, std::bind(&RidenHttpServer::handle_psu_get, this));
    server.on("/config/", HTTPMethod::HTTP_GET, std::bind(&RidenHttpServer::handle_config_get, this));
    server.on("/config/", HTTPMethod::HTTP_POST, std::bind(&RidenHttpServer::handle_config_post, this));
    server.on("/disconnect_client/", HTTPMethod::HTTP_POST, std::bind(&RidenHttpServer::handle_disconnect_client_post, this));
    server.on("/reboot/dongle/", HTTPMethod::HTTP_GET, std::bind(&RidenHttpServer::handle_reboot_dongle_get, this));
    server.on("/firmware/update/", HTTPMethod::HTTP_POST,
              std::bind(&RidenHttpServer::finish_firmware_update_post, this),
              std::bind(&RidenHttpServer::handle_firmware_update_post, this));
    server.on("/lxi/identification", HTTPMethod::HTTP_GET, std::bind(&RidenHttpServer::handle_lxi_identification, this));
    server.on("/qps/modbus/", HTTPMethod::HTTP_GET, std::bind(&RidenHttpServer::handle_modbus_qps, this));
    server.onNotFound(std::bind(&RidenHttpServer::handle_not_found, this));
    server.begin(port());

    if (MDNS.isRunning() && modbus.is_connected()) {
        auto lxi_service = MDNS.addService(NULL, "lxi", "tcp", port()); // allows discovery by lxi-tools
        MDNS.addServiceTxt(lxi_service, "path", "/");
        auto http_service = MDNS.addService(NULL, "http", "tcp", port());
        MDNS.addServiceTxt(http_service, "path", "/");
    }

    return true;
}

void RidenHttpServer::loop(void)
{
    server.handleClient();
}

uint16_t RidenHttpServer::port()
{
    return HTTP_RAW_PORT;
}

void RidenHttpServer::handle_root_get()
{
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", HTML_HEADER);
    if (modbus.is_connected()) {
        send_dongle_info();
        send_power_supply_info();
        send_network_info();
        send_services();
        send_connected_clients();
    } else {
        server.sendContent(HTML_NO_CONNECTION_BODY);
    }
    server.sendContent(HTML_FOOTER);
    server.sendContent("");
}

void RidenHttpServer::handle_psu_get()
{
    AllValues all_values;

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", HTML_HEADER);
    if (modbus.is_connected() && modbus.get_all_values(all_values)) {
        server.sendContent("        <div class='box'>");
        server.sendContent("            <a style='float:right' href='.'>Refresh</a><h2>Power Supply Details</h2>");
        server.sendContent("            <table class='info'>");
        server.sendContent("                <tbody>");
        send_info_row("Output", all_values.output_on ? "On" : "Off");
        send_info_row("Set", voltage_to_string(all_values.voltage_set) + " / " + current_to_string(all_values.current_set));
        send_info_row("Out",
                      voltage_to_string(all_values.voltage_out) + " / " + current_to_string(all_values.current_out) + " / " + power_to_string(all_values.power_out));
        send_info_row("Protection", protection_to_string(all_values.protection));
        send_info_row("Output Mode", outputmode_to_string(all_values.output_mode));
        send_info_row("Current Range", String(all_values.current_range, 10));
        send_info_row("Battery Mode", all_values.is_battery_mode ? "Yes" : "No");
        send_info_row("Voltage Battery", voltage_to_string(all_values.voltage_battery));
        send_info_row("Ah", String(all_values.ah, 3) + " Ah");
        send_info_row("Wh", String(all_values.wh, 3) + " Wh");
        server.sendContent("                </tbody>");
        server.sendContent("            </table>");
        server.sendContent("        </div>");

        server.sendContent("        <div class='box'>");
        server.sendContent("            <h2>Environment</h2>");
        server.sendContent("            <table class='info'>");
        server.sendContent("                <tbody>");
        send_info_row("Voltage In", voltage_to_string(all_values.voltage_in));
        send_info_row("System Temperature", String(all_values.system_temperature_celsius, 0) + "&deg;C" + " / " + String(all_values.system_temperature_fahrenheit, 0) + "&deg;F");
        send_info_row("Probe Temperature", String(all_values.probe_temperature_celsius, 0) + "&deg;C" + " / " + String(all_values.probe_temperature_fahrenheit, 0) + "&deg;F");
        server.sendContent("                </tbody>");
        server.sendContent("            </table>");
        server.sendContent("        </div>");

        server.sendContent("        <div class='box'>");
        server.sendContent("            <h2>Settings</h2>");
        server.sendContent("            <table class='info'>");
        server.sendContent("                <tbody>");
        send_info_row("Keypad Locked", all_values.keypad_locked ? "Yes" : "No");
        char clock_string[20];
        sprintf(clock_string, "%04u-%02u-%02u %02u:%02u:%02u",
                all_values.clock.tm_year + 1900,
                all_values.clock.tm_mon + 1,
                all_values.clock.tm_mday,
                all_values.clock.tm_hour,
                all_values.clock.tm_min,
                all_values.clock.tm_sec);
        send_info_row("Time", clock_string);
        send_info_row("Take OK", all_values.is_take_ok ? "Yes" : "No");
        send_info_row("Take Out", all_values.is_take_out ? "Yes" : "No");
        send_info_row("Power on boot", all_values.is_power_on_boot ? "Yes" : "No");
        send_info_row("Buzzer enabled", all_values.is_buzzer_enabled ? "Yes" : "No");
        send_info_row("Logo", all_values.is_logo ? "Yes" : "No");
        send_info_row("Language", language_to_string(all_values.language));
        send_info_row("Brightness", String(all_values.brightness, 10));
        server.sendContent("                </tbody>");
        server.sendContent("            </table>");
        server.sendContent("        </div>");

        server.sendContent("        <div class='box'>");
        server.sendContent("            <h2>Calibration</h2>");
        server.sendContent("            <table class='info'>");
        server.sendContent("                <tbody>");
        send_info_row("V_OUT_ZERO", String(all_values.calibration.V_OUT_ZERO, 10));
        send_info_row("V_OUT_SCALE", String(all_values.calibration.V_OUT_SCALE, 10));
        send_info_row("V_BACK_ZERO", String(all_values.calibration.V_BACK_ZERO, 10));
        send_info_row("V_BACK_SCALE", String(all_values.calibration.V_BACK_SCALE, 10));
        send_info_row("I_OUT_ZERO", String(all_values.calibration.I_OUT_ZERO, 10));
        send_info_row("I_OUT_SCALE", String(all_values.calibration.I_OUT_SCALE, 10));
        send_info_row("I_BACK_ZERO", String(all_values.calibration.I_BACK_ZERO, 10));
        send_info_row("I_BACK_SCALE", String(all_values.calibration.I_BACK_SCALE, 10));
        server.sendContent("                </tbody>");
        server.sendContent("            </table>");
        server.sendContent("        </div>");

        server.sendContent("        <div class='box'>");
        server.sendContent("            <h2>Presets</h2>");
        server.sendContent("            <table class='info'>");
        server.sendContent("                <tbody>");
        for (int preset = 0; preset < NUMBER_OF_PRESETS; preset++) {
            server.sendContent("<tr><th colspan='2' style='text-align:left'>Preset " + String(preset + 1, 10) + " (M" + String(preset + 1, 10) + ")" + "</th></tr>");
            send_info_row("Preset Voltage", voltage_to_string(all_values.presets[preset].voltage));
            send_info_row("Preset Current", current_to_string(all_values.presets[preset].current));
            send_info_row("Preset OVP", voltage_to_string(all_values.presets[preset].over_voltage_protection));
            send_info_row("Preset OCP", current_to_string(all_values.presets[preset].over_current_protection));
        }
        server.sendContent("                </tbody>");
        server.sendContent("            </table>");
        server.sendContent("        </div>");
    } else {
        server.sendContent(HTML_NO_CONNECTION_BODY);
    }
    server.sendContent(HTML_FOOTER);
    server.sendContent("");
}

void RidenHttpServer::handle_config_get()
{
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", HTML_HEADER);
    send_as_chunks(HTML_CONFIG_BODY_1);
    String configured_tz = riden_config.get_timezone_name();
    for (int i = 0; i < riden_config.get_number_of_timezones(); i++) {
        const Timezone &timezone = riden_config.get_timezone(i);
        String name = timezone.name;
        if (name == configured_tz) {
            server.sendContent("<option value='" + name + "' selected>" + name + "</option>");
        } else {
            server.sendContent("<option value='" + name + "'>" + name + "</option>");
        }
    }
    send_as_chunks(HTML_CONFIG_BODY_2);
    uint32_t uart_baudrate = riden_config.get_uart_baudrate();
    for (uint32_t option : uart_baudrates) {
        String option_string(option, 10);
        if (option == uart_baudrate) {
            server.sendContent("<option value='" + option_string + "' selected>" + option_string + "</option>");
        } else {
            server.sendContent("<option value='" + option_string + "'>" + option_string + "</option>");
        }
    }
    send_as_chunks(HTML_CONFIG_BODY_3);
    server.sendContent(HTML_FOOTER);
    server.sendContent("");
}

void RidenHttpServer::handle_config_post()
{
    String tz = server.arg("timezone");
    String uart_baudrate_string = server.arg("uart_baudrate");
    uint32_t uart_baudrate = std::strtoull(uart_baudrate_string.c_str(), nullptr, 10);
    LOG_F("Selected timezone: %s\r\n", tz.c_str());
    LOG_F("Selected baudrate: %u\r\n", uart_baudrate);
    riden_config.set_timezone_name(tz);
    riden_config.set_uart_baudrate(uart_baudrate);
    riden_config.commit();

    send_redirect_self();
}

void RidenHttpServer::handle_firmware_update_post()
{
    HTTPUpload &upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        // if(_debug) Serial.setDebugOutput(true);
        uint32_t maxSketchSpace;

        WiFiUDP::stopAll();
        maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;

        if (!Update.begin(maxSketchSpace)) { // start with max available size
            Update.end();
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        Update.end(true);
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Update.end();
    }
    yield();
}

void RidenHttpServer::finish_firmware_update_post()
{
    if (Update.hasError()) {
        server.client().setNoDelay(true);
        server.setContentLength(CONTENT_LENGTH_UNKNOWN);
        server.send(200, "text/html", HTML_HEADER);
        server.sendContent(HTML_DONGLE_UPDATE_1);
        server.sendContent(Update.getErrorString());
        server.sendContent(HTML_DONGLE_UPDATE_2);
        server.sendContent(HTML_FOOTER);
        server.sendContent("");
    } else {
        server.client().setNoDelay(true);
        server.setContentLength(CONTENT_LENGTH_UNKNOWN);
        server.send(200, "text/html", HTML_HEADER);
        server.sendContent(HTML_REBOOTING_DONGLE_UPDATE_BODY);
        server.sendContent(HTML_FOOTER);
        server.sendContent("");
        delay(100);
        server.client().stop();
        ESP.restart();
    }
}

void RidenHttpServer::handle_disconnect_client_post()
{
    String ip_string = server.arg("ip");
    String protocol = server.arg("protocol");
    IPAddress ip;
    if (ip.fromString(ip_string)) {
        if (protocol == scpi_protocol) {
            scpi.disconnect_client(ip);
        } else if (protocol == modbustcp_protocol) {
            bridge.disconnect_client(ip);
        } else if (protocol == vxi11_protocol) {
            vxi_server.disconnect_client(ip);
        }
    }

    send_redirect_root();
}

void RidenHttpServer::handle_reboot_dongle_get()
{
    String config_arg = server.arg("config_portal");

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", HTML_HEADER);
    if (config_arg == "true") {
        riden_config.set_config_portal_on_boot();
        riden_config.commit();
        server.sendContent(HTML_REBOOTING_DONGLE_CONFIG_PORTAL_BODY_1);
        server.sendContent(WiFi.getHostname());
        server.sendContent(HTML_REBOOTING_DONGLE_CONFIG_PORTAL_BODY_2);
    } else {
        server.sendContent(HTML_REBOOTING_DONGLE_BODY);
    }
    server.sendContent(HTML_FOOTER);
    server.sendContent("");
    delay(500);
    ESP.reset();
    delay(1000);
}

void RidenHttpServer::send_as_chunks(const char *str)
{
    const size_t chunk_length = 1000;
    size_t length = strlen(str);
    for (size_t start_pos = 0; start_pos < length; start_pos += chunk_length) {
        size_t end_pos = min(start_pos + chunk_length, length);
        server.sendContent(&(str[start_pos]), end_pos - start_pos);
        yield();
    }
}

void RidenHttpServer::send_redirect_root()
{
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "<html>");
    server.sendContent("<body>");
    server.sendContent("<script>");
    server.sendContent("  window.location = '/';");
    server.sendContent("</script>");
    server.sendContent("</body>");
    server.sendContent("</html>");
    server.sendContent("");
}

void RidenHttpServer::send_redirect_self()
{
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "<html>");
    server.sendContent("<body>");
    server.sendContent("<script>");
    server.sendContent("  location.replace('");
    server.sendContent(server.uri());
    server.sendContent("');");
    server.sendContent("</script>");
    server.sendContent("</body>");
    server.sendContent("</html>");
    server.sendContent("");
}

void RidenHttpServer::send_dongle_info()
{
    server.sendContent("        <div class='box'>");
    server.sendContent("            <h2>Riden Dongle</h2>");
    server.sendContent("            <table class='info'>");
    server.sendContent("                <tbody>");
    send_info_row("Version", RidenDongle::version_string);
    if (RidenDongle::build_time != nullptr) {
        send_info_row("Build Time", RidenDongle::build_time);
    }
    server.sendContent("                </tbody>");
    server.sendContent("            </table>");
    server.sendContent("        </div>");
}

void RidenHttpServer::send_power_supply_info()
{
    String type = modbus.get_type();

    server.sendContent("        <div class='box'>");
    server.sendContent("            <a style='float:right' href='/psu/'>Details</a><h2>Power Supply</h2>");
    server.sendContent("            <table class='info'>");
    server.sendContent("                <tbody>");
    send_info_row("Model", type);
    send_info_row("Firmware", get_firmware_version());
    send_info_row("Serial Number", get_serial_number());
    server.sendContent("                </tbody>");
    server.sendContent("            </table>");
    server.sendContent("        </div>");
}

void RidenHttpServer::send_network_info()
{
    server.sendContent("        <div class='box'>");
    server.sendContent("            <h2>Network Configuration</h2>");
    server.sendContent("            <table class='info'>");
    server.sendContent("                <tbody>");
    send_info_row("Hostname", WiFi.getHostname());
    send_info_row("MDNS", String(WiFi.getHostname()) + ".local");
    send_info_row("WiFi network", WiFi.SSID());
    send_info_row("IP", WiFi.localIP().toString());
    send_info_row("Subnet", WiFi.subnetMask().toString());
    send_info_row("Default Gateway", WiFi.gatewayIP().toString());
    for (int i = 0;; i++) {
        auto dns = WiFi.dnsIP(i);
        if (!dns.isSet()) {
            break;
        }
        send_info_row("DNS", dns.toString());
    }
    server.sendContent("                </tbody>");
    server.sendContent("            </table>");
    server.sendContent("        </div>");
}

void RidenHttpServer::send_services()
{
    server.sendContent("        <div class='box'>");
    server.sendContent("            <h2>Network Services</h2>");
    server.sendContent("            <table class='info'>");
    server.sendContent("                <tbody>");
    send_info_row("Web Server Port", String(this->port(), 10));
    send_info_row("Modbus TCP Port", String(bridge.port(), 10));
    send_info_row("VXI-11 Port", String(vxi_server.port(), 10));
    send_info_row("SCPI RAW Port", String(scpi.port(), 10));
    send_info_row("VISA Resource Address 1", vxi_server.get_visa_resource());
    send_info_row("VISA Resource Address 2", scpi.get_visa_resource());
    server.sendContent("                </tbody>");
    server.sendContent("            </table>");
    server.sendContent("        </div>");
}

void RidenHttpServer::send_connected_clients()
{
    server.sendContent("        <div class='box'>");
    server.sendContent("            <h2>Connected Clients</h2>");
    server.sendContent("            <table class='clients'>");
    server.sendContent("                <thead><tr>");
    server.sendContent("                <th>IP address</th>");
    server.sendContent("                <th>Protocol</th>");
    server.sendContent("                <th></th>");
    server.sendContent("                </tr></thead>");
    server.sendContent("                <tbody>");
    for (auto const &ip : vxi_server.get_connected_clients()) {
        send_client_row(ip, vxi11_protocol);
    }    
    for (auto const &ip : scpi.get_connected_clients()) {
        send_client_row(ip, scpi_protocol);
    }
    for (auto const &ip : bridge.get_connected_clients()) {
        send_client_row(ip, modbustcp_protocol);
    }
    server.sendContent("                </tbody>");
    server.sendContent("            </table>");
    server.sendContent("        </div>");
}

void RidenHttpServer::send_client_row(const IPAddress &ip, const String protocol)
{
    server.sendContent("<tr>");
    server.sendContent("<td>");
    server.sendContent(ip.toString());
    server.sendContent("</td>");
    server.sendContent("<td>");
    server.sendContent(protocol);
    server.sendContent("</td>");
    server.sendContent("<td><form method='post' action='/disconnect_client/'>");
    server.sendContent("<input type='hidden' name='ip' value='" + ip.toString() + "'>");
    server.sendContent("<input type='hidden' name='protocol' value='" + protocol + "'>");
    server.sendContent("<input type='submit' value='Disconnect'>");
    server.sendContent("</form></td>");
    server.sendContent("</tr>");
}

void RidenHttpServer::send_info_row(const String key, const String value)
{
    server.sendContent("                    <tr>");
    server.sendContent("                        <th>");
    server.sendContent(key);
    server.sendContent("</th>");
    server.sendContent("                        <td>");
    server.sendContent(value);
    server.sendContent("</td>");
    server.sendContent("                    </tr>");
}

void RidenHttpServer::handle_not_found()
{
    server.send(404, "text/plain", "404: Not found");
}

void RidenHttpServer::handle_modbus_qps()
{
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", HTML_HEADER);
    unsigned long start = millis();
    double voltage;
    for (int i = 0; i < 200; i++) {
        modbus.get_voltage_set(voltage);
    }
    unsigned long end = millis();
    double qps = 1000.0 * double(100) / double(end - start);
    LOG_F("qps = %f\r\n", qps);
    server.sendContent("<p>Result = ");
    server.sendContent(String(qps, 1));
    server.sendContent(" queries/second</p>");
    server.sendContent(HTML_FOOTER);
    server.sendContent("");
}

void RidenHttpServer::handle_lxi_identification()
{
    String model = modbus.get_type();
    String ip = WiFi.localIP().toString();
    String subnet_mask = WiFi.subnetMask().toString();
    String mac_address = WiFi.macAddress();
    String gateway = WiFi.gatewayIP().toString();
    // The values to be substituted
    const char *values[] = {
        model.c_str(),
        get_serial_number(),
        get_firmware_version(),
        WiFi.getHostname(),
        ip.c_str(),
        subnet_mask.c_str(),
        mac_address.c_str(),
        gateway.c_str(),
        scpi.get_visa_resource(),
        0 // Guard against wrong parameters, such as ${9999}
    };
    TinyTemplateEngineMemoryReader reader(LXI_IDENTIFICATION_TEMPLATE);
    reader.keepLineEnds(true);

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/xml");
    TinyTemplateEngine engine(reader);
    engine.start(values);
    while (const char *line = engine.nextLine()) {
        server.sendContent(line);
    }
    engine.end();
    server.sendContent(""); // Done
}

const char *RidenHttpServer::get_firmware_version()
{
    static char firmware_version_string[10];

    uint16_t firmware_version;
    modbus.get_firmware_version(firmware_version);
    sprintf(firmware_version_string, "%u.%u", firmware_version / 100u, firmware_version % 100u);
    return firmware_version_string;
}

const char *RidenHttpServer::get_serial_number()
{
    static char serial_number_string[10];
    uint32_t serial_number;
    modbus.get_serial_number(serial_number);
    sprintf(serial_number_string, "%08u", serial_number);
    return serial_number_string;
}

