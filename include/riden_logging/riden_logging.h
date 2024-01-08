// SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
//
// SPDX-License-Identifier: MIT

#ifdef MODBUS_USE_SOFWARE_SERIAL
#include <Arduino.h>
#define LOG(a) Serial.print(a)
#define LOG_LN(a) Serial.println(a)
#define LOG_F(...) Serial.printf(__VA_ARGS__)
#else
#define LOG(a)
#define LOG_LN(a)
#define LOG_F(...)
#endif
