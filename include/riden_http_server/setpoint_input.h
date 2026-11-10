#pragma once

#include <Arduino.h>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace RidenDongle {

// Accept one complete decimal number. A malformed/empty body must never
// become zero, and NaN/overflow must not reach an integer register conversion.
inline bool parse_setpoint_input(const String &input, double maximum, double &value)
{
    const char *first = input.c_str();
    const char *limit = first + input.length();
    while (first < limit && std::isspace(static_cast<unsigned char>(*first))) ++first;
    const char *last = limit;
    while (last > first && std::isspace(static_cast<unsigned char>(last[-1]))) --last;
    if (first == last) return false;
    for (const char *p = first; p < last; ++p) {
        if (!std::isdigit(static_cast<unsigned char>(*p)) &&
            *p != '+' && *p != '-' && *p != '.' && *p != 'e' && *p != 'E') return false;
    }
    errno = 0;
    char *end = nullptr;
    const double parsed = std::strtod(first, &end);
    if (end != last || errno == ERANGE || !std::isfinite(parsed) || parsed < 0 || parsed > maximum)
        return false;
    value = parsed;
    return true;
}

} // namespace RidenDongle
