// SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
//
// SPDX-License-Identifier: MIT

#include "http_static.h"
#include <riden_config/riden_config.h>
#include <riden_http_server/riden_http_server.h>
#include <riden_logging/riden_logging.h>

#include <ESP8266mDNS.h>
#include <list>

using namespace RidenDongle;

static String voltageToString(double voltage)
{
    if (voltage < 1) {
        return String(voltage * 1000, 0) + " mV";
    } else {
        return String(voltage, 3) + " V";
    }
}

static String currentToString(double current)
{
    if (current < 1) {
        return String(current * 1000, 0) + " mA";
    } else {
        return String(current, 3) + " A";
    }
}

static String powerToString(double power)
{
    if (power < 1) {
        return String(power * 1000, 0) + " mW";
    } else {
        return String(power, 3) + " W";
    }
}

static String protectionToString(Protection protection)
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

static String outputModeToString(OutputMode output_mode)
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

bool RidenHttpServer::begin()
{
    MDNS.addService("lxi", "tcp", HTTP_RAW_PORT); // allows discovery by lxi-tools
    MDNS.addService("http", "tcp", HTTP_RAW_PORT);

    server.on("/", HTTPMethod::HTTP_GET, std::bind(&RidenHttpServer::get_root, this));
    server.on("/", HTTPMethod::HTTP_POST, std::bind(&RidenHttpServer::post_root, this));
    server.on("/psu/", HTTP_GET, std::bind(&RidenHttpServer::handle_get_psu, this));
    server.on("/config/", HTTPMethod::HTTP_GET, std::bind(&RidenHttpServer::get_config, this));
    server.on("/config/", HTTPMethod::HTTP_POST, std::bind(&RidenHttpServer::post_config, this));
    server.on("/reboot/dongle/", HTTPMethod::HTTP_GET, std::bind(&RidenHttpServer::reboot_dongle, this));
    server.onNotFound(std::bind(&RidenHttpServer::handle_not_found, this));
    server.begin();

    return true;
}

void RidenHttpServer::loop(void)
{
    server.handleClient();
}

void RidenHttpServer::get_root()
{
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", HTML_HEADER);
    if (modbus.is_connected()) {
        send_connected_clients();
        send_power_supply_info();
        send_network_info();
        send_services();
    } else {
        server.sendContent(HTML_NO_CONNECTION_BODY);
    }
    server.sendContent(HTML_FOOTER);
    server.sendContent("");
}

void RidenHttpServer::handle_get_psu()
{
    AllValues all_values;

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", HTML_HEADER);
    if (modbus.get_all_values(all_values)) {
        server.sendContent("        <div class='box'>");
        server.sendContent("            <a style='float:right' href='.'>Refresh</a><h2>Power Supply Details</h2>");
        server.sendContent("            <table class='info'>");
        server.sendContent("                <tbody>");
        send_info_row("Output", all_values.output_on ? "On" : "Off");
        send_info_row("Set", voltageToString(all_values.voltage_set) + " / " + currentToString(all_values.current_set));
        send_info_row("Out",
                      voltageToString(all_values.voltage_out) + " / " + currentToString(all_values.current_out) + " / " + powerToString(all_values.power_out));
        send_info_row("Protection", protectionToString(all_values.protection));
        send_info_row("Output Mode", outputModeToString(all_values.output_mode));
        send_info_row("Current Range", String(all_values.current_range, 10));
        send_info_row("Battery Mode", all_values.is_battery_mode ? "Yes" : "No");
        send_info_row("Voltage Battery", voltageToString(all_values.voltage_battery));
        send_info_row("Ah", String(all_values.ah, 3) + " Ah");
        send_info_row("Wh", String(all_values.wh, 3) + " Wh");
        server.sendContent("                </tbody>");
        server.sendContent("            </table>");
        server.sendContent("        </div>");
        server.sendContent("</tbody>");
        server.sendContent("</table>");

        server.sendContent("        <div class='box'>");
        server.sendContent("            <h2>Environment</h2>");
        server.sendContent("            <table class='info'>");
        server.sendContent("                <tbody>");
        send_info_row("Voltage In", voltageToString(all_values.voltage_in));
        send_info_row("System Temperature", String(all_values.system_temperature_celsius, 0) + "&deg;C" + " / " + String(all_values.system_temperature_fahrenheit, 0) + "&deg;F");
        send_info_row("Probe Temperature", String(all_values.probe_temperature_celsius, 0) + "&deg;C" + " / " + String(all_values.probe_temperature_fahrenheit, 0) + "&deg;F");
        server.sendContent("                </tbody>");
        server.sendContent("            </table>");
        server.sendContent("        </div>");
        server.sendContent("</tbody>");
        server.sendContent("</table>");

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
        send_info_row("Language", String(all_values.language, 10));
        send_info_row("Brightness", String(all_values.brightness, 10));
        server.sendContent("                </tbody>");
        server.sendContent("            </table>");
        server.sendContent("        </div>");
        server.sendContent("</tbody>");
        server.sendContent("</table>");

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
        server.sendContent("</tbody>");
        server.sendContent("</table>");

        server.sendContent("        <div class='box'>");
        server.sendContent("            <h2>Presets</h2>");
        server.sendContent("            <table class='info'>");
        server.sendContent("                <tbody>");
        for (int preset = 0; preset < NUMBER_OF_PRESETS; preset++) {
            server.sendContent("<tr><th colspan='2' style='text-align:left'>Preset " + String(preset + 1, 10) + " (M" + String(preset + 1, 10) + ")" + "</th></tr>");
            send_info_row("Preset Voltage", voltageToString(all_values.presets[preset].voltage));
            send_info_row("Preset Current", currentToString(all_values.presets[preset].current));
            send_info_row("Preset OVP", voltageToString(all_values.presets[preset].over_voltage_protection));
            send_info_row("Preset OCP", currentToString(all_values.presets[preset].over_current_protection));
        }
        server.sendContent("                </tbody>");
        server.sendContent("            </table>");
        server.sendContent("        </div>");
        server.sendContent("</tbody>");
        server.sendContent("</table>");
    } else {
        server.sendContent(HTML_NO_CONNECTION_BODY);
    }
    server.sendContent(HTML_FOOTER);
    server.sendContent("");
}

void RidenHttpServer::get_config()
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
    server.sendContent(HTML_FOOTER);
    server.sendContent("");
}

void RidenHttpServer::post_config()
{
    String tz = server.arg("timezone");
    LOG_F("Selected timezone: %s\r\n", tz.c_str());
    riden_config.set_timezone_name(tz);
    riden_config.commit();

    send_redirect_self();
}

void RidenHttpServer::reboot_dongle()
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
    }
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

void RidenHttpServer::send_connected_clients()
{
    return;
    server.sendContent("<h1>Connected Clients</h1>");
    server.sendContent("<table>");
    server.sendContent("<thead><tr><th>IP</th><th>Interface</th><th>Action</th></tr></thead>");

    server.sendContent("<tbody>");
    server.sendContent("<form method='post'>");
    for (auto const &client : scpi.get_connected_clients()) {
        server.sendContent("<tr>");
        // IP
        server.sendContent("<td>");
        server.sendContent(client);
        server.sendContent("</td>");
        // Type
        server.sendContent("<td>SCPI</td>");
        // Disconnect action
        server.sendContent("<td><input type='submit' name='disconnect_ip' value='");
        server.sendContent(client);
        server.sendContent("'></td>");
        server.sendContent("</tr>");
    }
    server.sendContent("</form>");
    server.sendContent("</tbody>");

    server.sendContent("</table>");
}

void RidenHttpServer::send_power_supply_info()
{
    uint16_t firmware_version;
    uint32_t serial_number;
    String type = modbus.get_type();
    modbus.get_firmware_version(firmware_version);
    modbus.get_serial_number(serial_number);
    char tmp_string[10];

    server.sendContent("        <div class='box'>");
    server.sendContent("            <a style='float:right' href='/psu/'>Details</a><h2>Power Supply</h2>");
    server.sendContent("            <table class='info'>");
    server.sendContent("                <tbody>");
    send_info_row("Model", type);
    sprintf(tmp_string, "%u.%u", firmware_version / 100, firmware_version % 100);
    send_info_row("Firmware", tmp_string);
    sprintf(tmp_string, "%08d", serial_number);
    send_info_row("Serial Number", String(tmp_string));
    server.sendContent("                </tbody>");
    server.sendContent("            </table>");
    server.sendContent("        </div>");
    server.sendContent("</tbody>");
    server.sendContent("</table>");
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
    server.sendContent("</tbody>");
    server.sendContent("</table>");
}

void RidenHttpServer::send_services()
{
    server.sendContent("        <div class='box'>");
    server.sendContent("            <h2>Network Services</h2>");
    server.sendContent("            <table class='info'>");
    server.sendContent("                <tbody>");
    send_info_row("Web Server Port", String(HTTP_RAW_PORT, 10));
    send_info_row("Modbus TCP Port", String(bridge.port(), 10));
    send_info_row("SCPI Port", String(scpi.port(), 10));
    server.sendContent("                </tbody>");
    server.sendContent("            </table>");
    server.sendContent("        </div>");
    server.sendContent("</tbody>");
    server.sendContent("</table>");
}

void RidenHttpServer::post_root()
{
    String ip = server.arg("disconnect_ip");
    LOG_F("Disconnect ");
    LOG_LN(ip);
    scpi.disconnect_client(ip);

    send_redirect_self();
}

void RidenHttpServer::send_info_row(String key, String value)
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
