#include <riden_modbus_bridge/riden_modbus_bridge.h>
#include <cassert>
#include <iostream>

uint64_t host_time_us=10000;
HostSerial Serial;

static void emulate_psu(const std::vector<uint8_t> &request) {
    const uint16_t reg=(uint16_t(request[2])<<8)|request[3];
    const uint16_t count=(uint16_t(request[4])<<8)|request[5];
    assert(request[1]==3 && count<=20);
    std::vector<uint8_t> response={1,3,uint8_t(count*2)};
    for(uint16_t i=0;i<count;++i) {
        const uint16_t value=reg+i==0 ? 60065 : reg+i==8 ? 2600 : reg+i==10 ? 2599 : 0;
        response.push_back(value>>8);response.push_back(value);
    }
    uint16_t crc=0xffff;
    for(uint8_t byte:response) {
        crc ^= byte;
        for(unsigned i=0;i<8;++i) crc=crc&1 ? (crc>>1)^0xa001 : crc>>1;
    }
    response.push_back(crc);response.push_back(crc>>8);
    Serial.deliver(std::move(response));
}

static void query(const std::shared_ptr<HostSocket> &socket, uint16_t id, uint16_t reg) {
    const uint8_t packet[]={uint8_t(id>>8),uint8_t(id),0,0,0,6,1,3,
                            uint8_t(reg>>8),uint8_t(reg),0,1};
    socket->incoming.insert(socket->incoming.end(),std::begin(packet),std::end(packet));
}

static void check(const std::shared_ptr<HostSocket> &socket, uint16_t id, uint16_t value) {
    assert(!socket->outgoing.empty());
    const auto &packet=socket->outgoing.back();
    const std::vector<uint8_t> expected={uint8_t(id>>8),uint8_t(id),0,0,0,5,1,3,2,
                                        uint8_t(value>>8),uint8_t(value)};
    assert(packet==expected);
}

int main() {
    Serial.on_request=emulate_psu;
    static RidenDongle::RidenModbus bus;assert(bus.begin());
    RidenDongle::RidenModbusBridge bridge(bus);assert(bridge.begin());
    auto &server=*HostNetworkServer::instance;
    auto first=server.connect(100), second=server.connect(100), ha=server.connect(135);
    bridge.loop();
    assert(bridge.get_connected_clients().size()==3);
    for(unsigned cycle=0;cycle<200;++cycle) {
        // Separate TCP processes may reuse exactly the same transaction ID.
        query(first,cycle,8);query(second,cycle,10);query(ha,cycle,0);
        for(unsigned i=0;i<5;++i) bridge.loop();
        check(first,cycle,2600);check(second,cycle,2599);check(ha,cycle,60065);
        uint16_t web[20];assert(bus.read_holding_registers(0,web,20));
        assert(web[0]==60065 && web[8]==2600 && web[10]==2599);
    }
    first->open=false;
    auto replacement=server.connect(100);bridge.loop();
    assert(bridge.get_connected_clients().size()==3);
    query(replacement,0,0);query(second,0,10);query(ha,0,8);
    for(unsigned i=0;i<5;++i) bridge.loop();
    check(replacement,0,60065);check(second,0,2599);check(ha,0,2600);
    std::cout<<"PASS: real TCP parser/accept loop, two same-IP clients + HA + web, reconnect\n";
}
