// SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
//
// SPDX-License-Identifier: MIT

#pragma once

static const char *HTML_HEADER =
    "<html>"
    "    <head>"
    "        <title>Riden Multi-Purpose WiFi Dongle</title>"
    "        <style>"
    "            body {"
    "                font-family: sans-serif;"
    "                width: 50%;"
    "                min-width: 320px;"
    "                margin: 10px auto 0 auto;"
    "                color: #383428;"
    "            }"
    "            div {"
    "               margin-bottom: 6px;"
    "            }"
    "            div.header {"
    "                margin-bottom: 10px;"
    "            }"
    "            a#configure {float:right;}"
    "            div.box {"
    "                background-color: #E4E4E2;"
    "                margin-top: 10px;"
    "                padding: 4px;"
    "            }"
    "            div.danger {"
    "                background-color: #ED2939;"
    "            }"
    "            .danger {"
    "                color: white;"
    "            }"
    "            .danger a, .danger a:hover, .danger a:active, .danger a:visited {"
    "                color: white;"
    "            }"
    "            .box table {"
    "                width: 100%;"
    "            }"
    "            .box h2 {"
    "                text-align: center;"
    "                margin: 0 0 8px 0;"
    "            }"
    "            .box td,th {"
    "                text-align: left;"
    "            }"
    "            table.info th {text-align: right; padding-right: 10px; width:50%;}"
    "            table.info td {text-align: left; padding-left: 10px; width:50%;}"
    "            table.clients th {text-align: left}"
    "            table.clients td {text-align: left}"
    "            table.clients form {display: inline}"
    "        </style>"
    "    </head>"
    "    <body>"
    "        <a id='home' href='/'>Home</a>"
    "        <a id='configure' href='/config/'>Configure</a>";

static const char *HTML_FOOTER =
    "    </body>"
    "</html>";

static const char *HTML_CONFIG_BODY_1 =
    "<form method='post'>"
    "    <div class='box'>"
    "        <h2>Dongle Configuration</h2>"
    "        <table class='info'>"
    "            <tbody>"
    "                <tr>"
    "                    <th>Time Zone</th>"
    "                    <td><select name='timezone'>";

static const char *HTML_CONFIG_BODY_2 =
    "                    </select></td>"
    "                </tr>"
    "                <tr>"
    "                    <th>UART baudrate</th>"
    "                    <td><select name='uart_baudrate'>";

static const char *HTML_CONFIG_BODY_3 =
    "                    </select></td>"
    "                </tr>"
    "                <tr><th></th><td><input type='submit' value='Save'></td></tr>"
    "            </tbody>"
    "        </table>"
    "    </div>"
    "</form>"
    "<form method='post' action='/firmware/update/' enctype='multipart/form-data'>"
    "    <div class='box'>"
    "        <h2>Firmware Update</h2>"
    "        <table class='info'>"
    "            <tbody>"
    "                <tr>"
    "                    <th>Firmware .bin file</th>"
    "                    <td><input type='file' name='firmware' accept='.bin'></td>"
    "                </tr>"
    "                <tr><th></th><td><input type='submit' value='Update'></td></tr>"
    "            </tbody>"
    "        </table>"
    "    </div>"
    "</form>"
    "<div class='box danger'>"
    "    <h2>Danger Zone</h2>"
    "    <a href='/reboot/dongle/'>Reboot Dongle</a><br>"
    "    <a href='/reboot/dongle/?config_portal=true'>Reboot Dongle to Web Configuration Portal</a><br>"
    "</div>";

static const char *HTML_REBOOTING_DONGLE_BODY =
    "<div class='box'>"
    "    <h2>Dongle is Rebooting</h2>"
    "    <div><p>Redirecting to main page in <span id='counter'>10</span> seconds.</p>"
    "    <p><a href='/'>Return to main page</a></p>"
    "    </div>"
    "</div>"
    "<script>"
    "    setInterval(function() {"
    "        var div = document.querySelector('#counter');"
    "        var count = div.textContent * 1 - 1;"
    "        div.textContent = count;"
    "        if (count <= 0) {"
    "            window.location.replace('/');"
    "        }"
    "    }, 1000);"
    "</script>";

static const char *HTML_REBOOTING_DONGLE_UPDATE_BODY =
    "<div class='box'>"
    "    <h2>Updated Dongle Firmware</h2>"
    "    <div><p>The dongle is now rebooting.</p>"
    "    <p>Redirecting to main page in <span id='counter'>20</span> seconds.</p>"
    "    <p><a href='/'>Return to main page</a></p>"
    "    </div>"
    "</div>"
    "<script>"
    "    setInterval(function() {"
    "        var div = document.querySelector('#counter');"
    "        var count = div.textContent * 1 - 1;"
    "        div.textContent = count;"
    "        if (count <= 0) {"
    "            window.location.replace('/');"
    "        }"
    "    }, 1000);"
    "</script>";

static const char *HTML_DONGLE_UPDATE_1 =
    "<div class='box'>"
    "    <h2>Dongle Firmware Failed to Update</h2>"
    "    <div><p>The failure is: <b>";

static const char *HTML_DONGLE_UPDATE_2 =
    "</b>."
    "</div>";

static const char *HTML_REBOOTING_DONGLE_CONFIG_PORTAL_BODY_1 =
    "<div class='box'>"
    "    <h2>Dongle is Rebooting</h2>"
    "    <div>An access point named <b>";

static const char *HTML_REBOOTING_DONGLE_CONFIG_PORTAL_BODY_2 =
    "</b> should show up shortly.</div>"
    "    <p>Connect to it and the config portal will show up.</p>";

static const char *HTML_NO_CONNECTION_BODY =
    "<div class='box'>"
    "    <h2>Unable to communicate with power supply</h2>"
    "    <p>Make sure you have configured the power supply for TTL"
    " at the <a href='/config/'>configured</a> baudrate.</p>"
    "    <p>You must power-cycle the power supply if you modified"
    " its configuration.</p>"
    "</div>";
