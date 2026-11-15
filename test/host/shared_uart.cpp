#include <Arduino.h>
#include <ModbusTCP.h>
#include <riden_modbus/riden_modbus.h>
#include <riden_modbus_bridge/riden_modbus_bridge.h>
#include <cassert>
#include <iostream>

uint64_t host_time_us = 10000;
HostSerial Serial;
static uint16_t regs[512] = {};
static unsigned drop_next = 0;
static bool wrong_count_next = false;
static uint8_t exception_next = 0;
static uint64_t next_delay_us = 3000;
static bool enforce_turnaround = false;
static uint64_t reply_at = 0;
static bool corrupt_crc_next = false, drop_write_ack_next = false;
static unsigned writes = 0;

static std::vector<uint8_t> adu(uint8_t address, std::vector<uint8_t> pdu) {
    pdu.insert(pdu.begin(),address);
    uint16_t crc=0xffff;
    for (uint8_t byte:pdu) {
        crc ^= byte;
        for(int i=0;i<8;i++) crc = crc&1 ? (crc>>1)^0xa001 : crc>>1;
    }
    pdu.push_back(crc & 0xff); pdu.push_back(crc >> 8); return pdu;
}

static void emulate_psu(const std::vector<uint8_t> &req) {
    if (drop_next) { --drop_next; return; }
    if (enforce_turnaround && host_time_us - reply_at < 10000) return;
    const auto delay_us=next_delay_us; next_delay_us=3000;
    const uint8_t address=req[0], fn=req[1];
    if(exception_next) {
        Serial.deliver(adu(address,{uint8_t(fn|0x80),exception_next}),delay_us);
        exception_next=0; return;
    }
    const uint16_t start=(uint16_t(req[2])<<8)|req[3];
    const uint16_t count=(uint16_t(req[4])<<8)|req[5];
    std::vector<uint8_t> pdu;
    if(fn==3) {
        const uint16_t n=wrong_count_next ? 20 : count; wrong_count_next=false;
        pdu={3,uint8_t(n*2)};
        for(uint16_t i=0;i<n;i++) { pdu.push_back(regs[start+i]>>8);pdu.push_back(regs[start+i]&0xff); }
    } else if(fn==6) {
        ++writes;
        regs[start]=count; pdu.assign(req.begin()+1,req.begin()+6);
    } else if(fn==16) {
        ++writes;
        for(uint16_t i=0;i<count;i++) regs[start+i]=(uint16_t(req[7+i*2])<<8)|req[8+i*2];
        pdu.assign(req.begin()+1,req.begin()+6);
    } else { std::abort(); }
    if (drop_write_ack_next && (fn==6 || fn==16)) {
        drop_write_ack_next=false;
        return;
    }
    auto response=adu(address,pdu);
    if (corrupt_crc_next) { response.back() ^= 0xff; corrupt_crc_next=false; }
    Serial.deliver(std::move(response),delay_us);
    reply_at = host_time_us + delay_us;
}

static void pump(RidenDongle::RidenModbus &bus, unsigned ms=15) {
    for(unsigned i=0;i<ms;i++) { delay(1);bus.loop(); }
}

static void check_packet(const HostTCPClient &client, uint16_t id, const std::vector<uint8_t> &pdu,
                         uint8_t unit=1) {
    assert(!client.packets.empty());
    const auto &packet=client.packets.back();
    assert(packet.size()==pdu.size()+7);
    assert(packet[0]==uint8_t(id>>8) && packet[1]==uint8_t(id));
    assert(packet[2]==0 && packet[3]==0 && packet[6]==unit);
    assert(((uint16_t(packet[4])<<8)|packet[5])==pdu.size()+1);
    assert(std::vector<uint8_t>(packet.begin()+7,packet.end())==pdu);
}

int main(int argc,char **argv) {
    const bool reproduce = argc>1 && String(argv[1])=="--reproduce";
    regs[0]=60065;regs[2]=12345;regs[3]=143;regs[5]=32;
    regs[8]=2600;regs[9]=500;regs[10]=2599;regs[14]=6569;
    Serial.on_request=emulate_psu;
    static RidenDongle::RidenModbus bus; assert(bus.begin());
    RidenDongle::RidenModbusBridge bridge(bus);assert(bridge.begin());
    auto &tcp=*ModbusTCP::instance;
    auto &tc=tcp.addClient(100);auto &ha=tcp.addClient(135);

    // The next TCP request is being parsed when the previous UART response
    // arrives. Upstream rawResponce() frees its incoming buffer here.
    tcp.enqueue(100,17,{3,0,0,0,1});
    tcp.enqueue(135,23,{3,0,8,0,2});
    bridge.loop();
    pump(bus);
    check_packet(tc,17,{3,2,0xea,0xa1});
    check_packet(ha,23,{3,4,0x0a,0x28,0x01,0xf4});
    if(reproduce) return 0;
    std::cout<<"PASS: two TCP clients and overlapping UART completion\n";

#ifndef RIDEN_REPRODUCE_UPSTREAM
    // A concurrent web read must get its own register block.
    tcp.enqueue(100,31,{3,0,0,0,1});bridge.loop();
    uint16_t web[20];std::fill(std::begin(web),std::end(web),0xcafe);
    assert(bus.read_holding_registers(0,web,20));
    assert(web[0]==60065 && web[8]==2600 && web[9]==500 && web[14]==6569);
    check_packet(tc,31,{3,2,0xea,0xa1});
    std::cout<<"PASS: web register block while TCP request is pending\n";

    // A slow-to-rearm slave ignores back-to-back requests. No timeout
    // should be needed when TCP and local operations share a UART guard.
    enforce_turnaround=true;
    tcp.enqueue(100,32,{3,0,8,0,1});
    tcp.enqueue(135,33,{3,0,10,0,1});bridge.loop();pump(bus);
    check_packet(tc,32,{3,2,0x0a,0x28});
    check_packet(ha,33,{3,2,0x0a,0x27});
    assert(bus.read_holding_registers(0,web,20));
    enforce_turnaround=false;
    std::cout<<"PASS: shared UART turnaround between TCP and web requests\n";

    auto &tc2=tcp.addClient(100); // second TestController on the same PC
    const auto tc_packets=tc.packets.size(), tc2_packets=tc2.packets.size();
    tcp.enqueueClient(tc,0,{3,0,8,0,1});
    tcp.enqueueClient(tc2,0,{3,0,10,0,1});bridge.loop();pump(bus);
    assert(tc.packets.size()==tc_packets+1 && tc2.packets.size()==tc2_packets+1);
    check_packet(tc,0,{3,2,0x0a,0x28});
    check_packet(tc2,0,{3,2,0x0a,0x27});
    std::cout<<"PASS: same-IP TCP clients with identical transaction IDs stay separate\n";

    drop_next=1;tcp.enqueue(100,0,{3,0,0,0,1});bridge.loop();pump(bus);
    check_packet(tc,0,{3,2,0xea,0xa1});
    std::cout<<"PASS: one lost TCP read is retried with the original transaction ID\n";

    const auto discarded=bus.get_diagnostics().discarded_bytes;
    next_delay_us=1020000;tcp.enqueueClient(tc2,34,{3,0,8,0,1});bridge.loop();
    check_packet(tc2,34,{3,2,0x0a,0x28});
    assert(bus.get_diagnostics().discarded_bytes>discarded);
    assert(bus.read_holding_registers(0,web,20) && web[0]==60065);
    std::cout<<"PASS: late reply drained during recovery before retry/web reads\n";

    // A fresh zero is data, including after a failed UART attempt.
    corrupt_crc_next=true;tcp.enqueueClient(tc2,35,{3,0,11,0,1});bridge.loop();
    check_packet(tc2,35,{3,2,0,0});
    std::cout<<"PASS: CRC failure retried, genuine zero preserved\n";

    drop_next=2;tcp.enqueue(100,0,{3,0,0,0,1});bridge.loop();
    assert(bus.read_holding_registers(0,web,20));
    check_packet(tc,0,{0x83,0x0b});
    assert(web[0]==60065 && web[8]==2600);
    std::cout<<"PASS: TCP timeout, transaction ID zero, next web reply not stolen\n";

    wrong_count_next=true;tcp.enqueue(135,41,{3,0,0,0,1});bridge.loop();pump(bus);
    check_packet(ha,41,{3,2,0xea,0xa1});
    assert(bus.read_holding_registers(0,web,20));assert(web[0]==60065);
    std::cout<<"PASS: mismatched reply length rejected, web recovers\n";

    const auto before_exception=Serial.requests.size();
    exception_next=2;tcp.enqueue(100,42,{3,0,0,0,1});bridge.loop();pump(bus);
    check_packet(tc,42,{0x83,2});
    assert(Serial.requests.size()==before_exception+1);
    uint16_t value=0xcafe;exception_next=2;
    assert(!bus.read_holding_registers(0,&value));assert(value==0xcafe);
    wrong_count_next=true;
    assert(!bus.read_holding_registers(0,&value));assert(value==0xcafe);
    std::cout<<"PASS: RTU exceptions forwarded/reported as failures\n";

    const auto requests=Serial.requests.size();
    tcp.enqueue(100,43,{3,0,0,0,1},0);bridge.loop();
    check_packet(tc,43,{0x83,0x0a},0);assert(Serial.requests.size()==requests);
    std::cout<<"PASS: broadcast unit rejected without arming an interceptor\n";

    drop_next=2;const auto started=host_time_us;
    assert(!bus.read_holding_registers(0,&value));
    assert(host_time_us-started>=1000000);
    next_delay_us=700000;assert(bus.read_holding_registers(0,&value));assert(value==60065);
    std::cout<<"PASS: local timeout drains transaction, 700ms reply stays in scope\n";

    assert(bus.write_holding_register(8,2700));assert(regs[8]==2700);
    uint16_t settings[]={2600,500};assert(bus.write_holding_registers(8,settings,2));
    assert(regs[8]==2600 && regs[9]==500);
    exception_next=3;assert(!bus.write_holding_register(8,2800));assert(regs[8]==2600);
    std::cout<<"PASS: single/multiple writes and failed write result\n";

    tcp.enqueue(100,44,{6,0,8,0x0a,0x8c});bridge.loop();pump(bus);
    check_packet(tc,44,{6,0,8,0x0a,0x8c});assert(regs[8]==2700);
    tcp.enqueue(135,45,{16,0,8,0,2,4,0x0a,0x28,0x01,0xf4});bridge.loop();pump(bus);
    check_packet(ha,45,{16,0,8,0,2});assert(regs[8]==2600 && regs[9]==500);
    std::cout<<"PASS: TCP single/multiple writes routed to their clients\n";

    const auto writes_before=writes;
    const auto requests_before=Serial.requests.size();
    drop_write_ack_next=true;tcp.enqueueClient(tc2,46,{6,0,8,0x0a,0x28});bridge.loop();
    check_packet(tc2,46,{0x86,0x0b});
    assert(writes==writes_before+1 && Serial.requests.size()==requests_before+1);
    std::cout<<"PASS: lost write acknowledgement does not repeat the applied write\n";

    // Many interleavings of TC, HA, and web reads must preserve routing.
    for(uint16_t i=0;i<200;i++) {
        if (i%17==0) drop_next=1;
        const uint16_t setpoint=2600+i;
        regs[8]=setpoint;
        tcp.enqueue(100,100+i,{3,0,8,0,1});
        tcp.enqueue(135,1000+i,{3,0,3,0,1});bridge.loop();
        tcp.enqueueClient(tc2,100+i,{3,0,10,0,1});bridge.loop();
        assert(bus.read_holding_registers(0,web,20));
        check_packet(tc,100+i,{3,2,uint8_t(setpoint>>8),uint8_t(setpoint)});
        check_packet(ha,1000+i,{3,2,0,143});
        check_packet(tc2,100+i,{3,2,0x0a,0x27});
        assert(web[0]==60065 && web[8]==setpoint);
    }
    assert(bridge.get_diagnostics().read_retries>=16);
    assert(bridge.get_diagnostics().recovered_reads>=15);
    assert(bridge.get_diagnostics().uart_timeouts>=15);
    assert(bridge.get_diagnostics().invalid_responses>=1);
    std::cout<<"PASS: 200 cycles of two same-IP TCs + HA + web with dropped replies\n";
#endif
}
