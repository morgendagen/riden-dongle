#pragma once
#include <Modbus.h>
#include <deque>

class IPAddress {
    uint32_t address;
  public:
    IPAddress(uint32_t value = 0) : address(value) {}
    operator uint32_t() const { return address; }
};

struct HostTCPClient {
    uint32_t ip;
    bool open = true;
    std::vector<std::vector<uint8_t>> packets;
    bool connected() const { return open; }
    IPAddress remoteIP() const { return ip; }
    void flush() {}
    size_t write(const uint8_t *p, size_t n) { packets.emplace_back(p, p+n); return n; }
};

// Faithful model of the library's ownership rules: rawResponce() replaces
// _frame, which is also the active incoming request during task()/onRaw().
class ModbusTCP : public Modbus {
  protected:
    HostTCPClient *tcpclient[MODBUSIP_MAX_CLIENTS] = {};
    int8_t n = -1;
    int8_t getMaster(IPAddress ip) {
        for (int i=0; i<MODBUSIP_MAX_CLIENTS; ++i)
            if (tcpclient[i] && tcpclient[i]->ip == uint32_t(ip)) return i;
        return -1;
    }
  public:
    struct Request { int8_t slot; uint16_t id; uint8_t unit; std::vector<uint8_t> pdu; };
    std::deque<Request> incoming;
    uint16_t transaction = 1;
    inline static ModbusTCP *instance = nullptr;
    ModbusTCP() { instance = this; }
    ~ModbusTCP() { for (auto client:tcpclient) delete client; }
    void server() {}
    HostTCPClient &addClient(uint32_t ip) {
        for (auto &client: tcpclient) if (!client) { client = new HostTCPClient{ip}; return *client; }
        std::abort();
    }
    void enqueue(uint32_t ip, uint16_t id, std::vector<uint8_t> pdu, uint8_t unit=1) {
        incoming.push_back({getMaster(ip),id,unit,std::move(pdu)});
    }
    void enqueueClient(HostTCPClient &client, uint16_t id, std::vector<uint8_t> pdu, uint8_t unit=1) {
        for (int8_t slot=0; slot<MODBUSIP_MAX_CLIENTS; ++slot)
            if (tcpclient[slot] == &client) {
                incoming.push_back({slot,id,unit,std::move(pdu)});
                return;
            }
        std::abort();
    }
    void task() {
        while (!incoming.empty()) {
            const auto req = incoming.front(); incoming.pop_front();
            n = req.slot;
            free(_frame); _len = req.pdu.size();
            _frame = static_cast<uint8_t *>(malloc(_len));
            std::memcpy(_frame, req.pdu.data(), _len);
            frame_arg_t source(req.unit, tcpclient[n]->remoteIP(), req.id, true);
            _cbRaw(_frame, _len, &source);
            free(_frame); _frame = nullptr; _len = 0;
        }
        n = -1;
    }
    void setTransactionId(uint16_t id) { transaction = id ? id : 1; }
    uint16_t rawResponce(IPAddress ip, uint8_t *data, uint16_t len, uint8_t unit) {
        free(_frame); _frame = static_cast<uint8_t *>(malloc(len)); _len = len;
        std::memcpy(_frame, data, len);
        int n = getMaster(ip);
        if (n >= 0) {
            std::vector<uint8_t> packet = {uint8_t(transaction>>8),uint8_t(transaction),0,0,
                uint8_t((len+1)>>8),uint8_t(len+1),unit};
            packet.insert(packet.end(), _frame, _frame+len);
            tcpclient[n]->write(packet.data(),packet.size());
        }
        free(_frame); _frame = nullptr; _len = 0; ++transaction; return n >= 0;
    }
    uint16_t errorResponce(IPAddress ip, FunctionCode fn, ResultCode error, uint8_t unit=255) {
        uint8_t data[] = {uint8_t(fn|0x80),uint8_t(error)};
        return rawResponce(ip,data,sizeof(data),unit);
    }
};
