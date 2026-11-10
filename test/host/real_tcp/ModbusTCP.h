#pragma once
#include <Arduino.h>
#include <deque>
#include <memory>

class IPAddress {
    uint32_t address;
  public:
    IPAddress(uint32_t value=0) : address(value) {}
    operator uint32_t() const { return address; }
};
#define INADDR_NONE 0xffffffffUL

struct HostSocket {
    uint32_t ip;
    bool open = true;
    std::deque<uint8_t> incoming;
    std::vector<std::vector<uint8_t>> outgoing;
};

class HostNetworkClient {
    std::shared_ptr<HostSocket> socket;
  public:
    HostNetworkClient() = default;
    explicit HostNetworkClient(std::shared_ptr<HostSocket> socket) : socket(std::move(socket)) {}
    operator bool() const { return socket && socket->open; }
    bool connected() const { return bool(*this); }
    IPAddress remoteIP() const { return socket ? socket->ip : 0; }
    int available() const { return socket ? socket->incoming.size() : 0; }
    int read() {
        if (!available()) return -1;
        int byte=socket->incoming.front();socket->incoming.pop_front();return byte;
    }
    size_t readBytes(uint8_t *p, size_t n) {
        size_t count=0;
        while(count<n && available()) p[count++]=read();
        return count;
    }
    size_t write(const uint8_t *p, size_t n) {
        socket->outgoing.emplace_back(p,p+n);return n;
    }
    void flush() {}
    void stop() { if(socket) socket->open=false; }
};

class HostNetworkServer {
    std::deque<HostNetworkClient> incoming;
  public:
    inline static HostNetworkServer *instance=nullptr;
    explicit HostNetworkServer(uint16_t) { instance=this; }
    void begin() {}
    std::shared_ptr<HostSocket> connect(uint32_t ip) {
        auto socket=std::make_shared<HostSocket>();socket->ip=ip;
        incoming.emplace_back(socket);return socket;
    }
    HostNetworkClient accept() {
        if(incoming.empty()) return {};
        auto client=incoming.front();incoming.pop_front();return client;
    }
};

// Compile the production TCP parser, accept loop, client slots and cleanup.
// Only WiFi sockets/time are simulated in this harness.
#include <ModbusTCPTemplate.h>
#include <ModbusAPI.h>
class ModbusTCP : public ModbusAPI<ModbusTCPTemplate<HostNetworkServer, HostNetworkClient>> {};
