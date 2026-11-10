#include <Arduino.h>
#include <riden_modbus/riden_modbus_registers.h>
#include <riden_http_server/setpoint_input.h>
#include <cassert>
#include <iostream>
#include <limits>

namespace RidenDongle {
// Unit-test boundary: the production handlers, write helpers and conversions are
// inserted by run_setpoint_tests.py. Only HTTP and register I/O are simulated.
class RidenModbus {
  public:
    bool connected=true, ranges=false, read_ok=true, write_ok=true;
    double v_max=60.1, i_max=6.0, v_multi=1000.0, i_multi=10000.0;
    uint16_t range=0, registers[128]={};
    unsigned writes=0, reads=0;
    bool is_connected() { return connected; }
    double get_max_voltage() { return v_max; }
    double get_max_current() { return i_max; }
    bool supports_current_ranges() { return ranges; }
    void update_i_multi_for_range(uint16_t value) { i_multi=(value & 1) ? 1000.0 : 10000.0; }
    bool read_holding_registers(Register reg,uint16_t *value) {
        assert(reg==Register::CurrentRange);++reads;
        if (!read_ok) return false;
        *value=range;return true;
    }
    bool write_holding_register(Register reg,uint16_t value) {
        ++writes;
        if (!write_ok) return false;
        registers[+reg]=value;return true;
    }
    bool set_voltage_set(double voltage);
    bool set_current_set(double current);
    bool set_current_range(uint16_t current_range);
    bool write_voltage(Register reg,double voltage);
    bool write_current(Register reg,double current);
    uint16_t voltage_to_value(double voltage);
    uint16_t current_to_value(double current);
};
class RidenHttpServer {
  public:
    RidenModbus modbus;
    struct Http {
        String plain,body;int status=0;
        String arg(const char *) { return plain; }
        void send(int code,const char *,const String &text) { status=code;body=text; }
    } server;
    void handle_set_i();
    void handle_set_v();
    void handle_set_i_range();
};
}
using namespace RidenDongle;

// PRODUCTION_SETPOINT_FUNCTIONS

int main() {
    RidenHttpServer http;
    auto reject=[&](const String &value,bool current=false) {
        const auto writes=http.modbus.writes;
        http.server.plain=value;
        if(current) http.handle_set_i();else http.handle_set_v();
        assert(http.server.status==400 && http.modbus.writes==writes);
    };
    const char *invalid[]={""," ","abc","1,2","2 V","NaN","inf","Infinity","-0.1","1e999","1e-999","0x1p2",".","1 2"};
    for(const char *text:invalid) {reject(text);reject(text,true);}
    std::cout<<"PASS: malformed, non-finite, negative and overflowing HTTP numbers cannot write registers\n";
    reject(String("2\0junk",6));reject(String("0\0",2),true);
    std::cout<<"PASS: embedded NUL and trailing bytes are rejected before I/O\n";
    reject("60.101");reject("6.001",true);
    std::cout<<"PASS: voltage/current above model limits cannot write registers\n";

    http.server.plain=" \t2.6\r\n";http.handle_set_v();
    assert(http.server.status==200 && http.modbus.registers[8]==2600);
    http.server.plain="5e-2";http.handle_set_i();
    assert(http.server.status==200 && http.modbus.registers[9]==500);
    http.server.plain="60.1";http.handle_set_v();
    assert(http.server.status==200 && http.modbus.registers[8]==60100);
    http.server.plain="6";http.handle_set_i();
    assert(http.server.status==200 && http.modbus.registers[9]==60000);
    http.server.plain="0";http.handle_set_v();http.handle_set_i();
    assert(http.server.status==200 && http.modbus.registers[8]==0 && http.modbus.registers[9]==0);
    std::cout<<"PASS: valid decimals, whitespace, exponent, exact limits and real zero reach the correct registers\n";

    auto before=http.modbus.writes;
    http.server.plain="0";http.handle_set_i_range();
    assert(http.server.status==400 && http.modbus.writes==before);
    http.modbus.ranges=true;
    for(const char *value:{"","abc","2","-1","1.0"," 1"}) {
        http.server.plain=value;http.handle_set_i_range();
        assert(http.server.status==400 && http.modbus.writes==before);
    }
    http.server.plain="1";http.handle_set_i_range();
    assert(http.server.status==200 && http.modbus.registers[20]==1);
    std::cout<<"PASS: unsupported/malformed current ranges are rejected; a supported range is written exactly\n";

    before=http.modbus.writes;
    for(double value:{-1.0,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}) {
        assert(!http.modbus.set_voltage_set(value) && !http.modbus.set_current_set(value));
    }
    assert(!http.modbus.set_voltage_set(61.0) && !http.modbus.set_current_set(6.1));
    assert(!http.modbus.set_current_range(2));
    assert(http.modbus.writes==before);
    std::cout<<"PASS: direct callers cannot bypass finite, nonnegative and model-range guards\n";

    http.modbus.v_max=100;
    assert(!http.modbus.set_voltage_set(70) && http.modbus.writes==before);
    http.modbus.i_max=12;http.modbus.ranges=false;
    assert(!http.modbus.set_current_set(7) && http.modbus.writes==before);
    std::cout<<"PASS: representability guards prevent uint16 wrapping even with larger model limits\n";

    http.modbus.ranges=true;http.modbus.read_ok=false;
    assert(!http.modbus.set_current_set(1) && http.modbus.writes==before);
    http.modbus.read_ok=true;http.modbus.range=1;
    assert(http.modbus.set_current_set(7) && http.modbus.registers[9]==7000);
    std::cout<<"PASS: a reported range-read failure stops the write; a known high range uses its divisor\n";

    http.modbus.connected=false;http.server.plain="2";before=http.modbus.writes;
    http.handle_set_v();assert(http.server.status==500 && http.modbus.writes==before);
    http.modbus.connected=true;http.modbus.write_ok=false;
    http.handle_set_v();assert(http.server.status==500);
    std::cout<<"PASS: connection and write failures are not reported as successful commands\n";
}
