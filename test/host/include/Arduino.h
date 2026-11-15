#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

using boolean = bool;
class String : public std::string {
  public:
    using std::string::string;
    using std::string::operator=;
    String(const std::string &value) : std::string(value) {}
    int compareTo(const String &value) const { return compare(value); }
    template <class T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
    String(T value) : std::string(std::to_string(value)) {}
    template <class T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    String(T value, unsigned base) : std::string(std::to_string(value)) {
        if (base != 10) std::abort();
    }
    String(double value, unsigned digits) {
        char text[96];
        std::snprintf(text, sizeof(text), "%.*f", int(digits), value);
        assign(text);
    }
};
using std::min;
using std::max;
#define PROGMEM
#define OUTPUT 1
#define HIGH 1
#define LOW 0
#define SERIAL_8N1 0
#define highByte(x) uint8_t((x) >> 8)
#define lowByte(x) uint8_t(x)
inline uint16_t makeWord(uint16_t value) { return value; }
inline uint16_t makeWord(uint8_t high, uint8_t low) { return (uint16_t(high)<<8)|low; }
#define word(...) makeWord(__VA_ARGS__)
#define pgm_read_word(p) (*(p))
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitRead(value, bit) (((value) >> (bit)) & 1U)

extern uint64_t host_time_us;
inline uint32_t micros() { return uint32_t(host_time_us); }
inline uint32_t millis() { return uint32_t(host_time_us / 1000); }
inline void delay(unsigned long ms) { host_time_us += ms * 1000; }
inline void delayMicroseconds(unsigned long us) { host_time_us += us; }
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}

class Stream {
  public:
    virtual ~Stream() = default;
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() { return -1; }
    virtual size_t write(uint8_t) = 0;
    virtual size_t write(const uint8_t *, size_t) = 0;
    virtual void flush() = 0;
};

class HostSerial : public Stream {
  public:
    struct Delivery { uint64_t at; std::vector<uint8_t> bytes; };
    std::vector<uint8_t> tx, rx;
    std::vector<Delivery> deliveries;
    std::vector<std::vector<uint8_t>> requests;
    std::function<void(const std::vector<uint8_t> &)> on_request;
    void begin(unsigned long, int) {}
    void deliver(std::vector<uint8_t> bytes, uint64_t after_us = 3000) {
        deliveries.push_back({host_time_us + after_us, std::move(bytes)});
    }
    int available() override {
        for (auto it = deliveries.begin(); it != deliveries.end();) {
            if (it->at <= host_time_us) {
                rx.insert(rx.end(), it->bytes.begin(), it->bytes.end());
                it = deliveries.erase(it);
            } else ++it;
        }
        return rx.size();
    }
    int read() override {
        if (!available()) return -1;
        const int b = rx.front(); rx.erase(rx.begin()); return b;
    }
    size_t write(uint8_t b) override { tx.push_back(b); return 1; }
    size_t write(const uint8_t *p, size_t n) override { tx.insert(tx.end(), p, p+n); return n; }
    void flush() override {
        if (tx.empty()) return;
        const auto request = tx; tx.clear(); requests.push_back(request);
        if (on_request) on_request(request);
    }
};
extern HostSerial Serial;
