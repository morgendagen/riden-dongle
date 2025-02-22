// SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
//
// SPDX-License-Identifier: MIT
#ifdef MOCK_RIDEN
#define MODBUS_USE_SOFWARE_SERIAL
#endif
#ifdef MODBUS_USE_SOFWARE_SERIAL
#include <Arduino.h>
#define LOG(a) Serial.print(a)
#define LOG_LN(a) Serial.println(a)
#define LOG_F(...) Serial.printf(__VA_ARGS__)
#define LOG_DUMP(buf,len) for (size_t i = 0; i < len; i++) { LOG_F("%02X ", buf[i]); }
#else
#define LOG(a)
#define LOG_LN(a)
#define LOG_F(...)
#define LOG_DUMP(buf,len)
#endif
