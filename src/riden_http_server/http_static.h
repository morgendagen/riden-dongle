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
    "            </tbody>"
    "        </table>"
    "        <input type='submit' value='Save'>"
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
    "    <div>Redirecting to main page in <span id='counter'>10</span> seconds.</div>"
    "    <a href='/'>Return to main page</a>"
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

static const char *HTML_REBOOTING_DONGLE_CONFIG_PORTAL_BODY_1 =
    "<div class='box'>"
    "    <h2>Dongle is Rebooting</h2>"
    "    <div>An access point named <b>";

static const char *HTML_REBOOTING_DONGLE_CONFIG_PORTAL_BODY_2 =
    "</b> should show up shortly.</div>"
    "    <p>Connect to it and the config portal will show up.</p>";

static const char *HTML_NO_CONNECTION_BODY =
    "<p><b>Unable to communicate with power supply.</b></p>"
    "<p>Make sure you have configured the power supply for TTL"
    " at 9600 bps.</p>"
    "<p>You must power-cycle the power supply if you modified"
    " its configuration.</p>";
