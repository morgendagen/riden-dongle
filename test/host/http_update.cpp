#include <Arduino.h>
#include <riden_logging/firmware_update_state.h>
#include "http_static.h"
#include <cassert>
#include <ctime>
#include <iostream>

uint64_t host_time_us=10000;
HostSerial Serial;
inline void yield() { delay(1); }
#define strlen_P std::strlen
static constexpr size_t CONTENT_LENGTH_UNKNOWN=size_t(-1);
enum UploadStatus { UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED };
struct HTTPUpload { UploadStatus status=UPLOAD_FILE_START;uint8_t buf[16]={};size_t currentSize=0; };
struct FakeESP {
    unsigned restarts=0;
    uint32_t free_space=0x200000;
    uint32_t getFreeSketchSpace() { return free_space; }
    void restart() { ++restarts; }
    void reset() { ++restarts; }
} ESP;
struct WiFiUDP { static unsigned stops;static void stopAll() { ++stops; } };
unsigned WiFiUDP::stops=0;
struct { String getHostname() { return "test-esp"; } } WiFi;
struct FakeUpdater {
    bool begin_ok=true,end_ok=true,error=false,partial=false;
    unsigned begins=0,finishes=0,cleanups=0;
    uint32_t size=0;
    bool begin(uint32_t n) { ++begins;size=n;return begin_ok; }
    size_t write(uint8_t *,size_t n) { return partial ? n-1 : n; }
    bool end(bool finish=false) { if(finish) ++finishes;else ++cleanups;return end_ok; }
    bool hasError() { return error; }
    String getErrorString() { return "mock updater error"; }
} Update;

namespace RidenDongle {
struct FakeConfig {
    unsigned portals=0,commits=0;
    void set_config_portal_on_boot() { ++portals; }
    void commit() { ++commits; }
} riden_config;
class RidenHttpServer {
  public:
    struct ClientState { unsigned stops=0,aborts=0,stop_timeout=0;bool stop_ok=true; } client_state;
    struct Client {
        ClientState *state;
        void setNoDelay(bool) {}
        bool stop(unsigned timeout=0) { ++state->stops;state->stop_timeout=timeout;return state->stop_ok; }
        void abort() { ++state->aborts; }
    };
    struct Http {
        RidenHttpServer *owner;
        HTTPUpload file;
        int status=0;
        String body,connection,portal="false";
        size_t content_length=0;
        HTTPUpload &upload() { return file; }
        Client client() { return {&owner->client_state}; }
        void sendHeader(const char *key,const char *value) { if(String(key)=="Connection") connection=value; }
        void setContentLength(size_t n) { content_length=n; }
        void send(int code,const char *,const String &content) { status=code;body=content; }
        void sendContent(const String &content) { body+=content; }
        void sendContent_P(const char *content) { body+=content; }
        String arg(const char *) { return portal; }
    } server;
    unsigned polls=0;
    std::function<void()> on_poll;
    RidenHttpServer() : server{this} {}
    void loop() { ++polls;if(on_poll) on_poll(); }
    bool restart_pending() const { return firmware_update_state().restart_pending; }
    void handle_firmware_update_post();
    void finish_firmware_update_post();
    void request_restart();
    bool handle_pending_restart();
    void handle_reboot_dongle_get();
};
}
using namespace RidenDongle;

// PRODUCTION_UPDATE_HANDLERS

static bool connected=true;
static RidenHttpServer http_server;
static unsigned clock_calls=0;
struct Service { unsigned calls=0;void loop() { ++calls; } void set_clock(const tm &) { ++clock_calls; } };
static Service riden_modbus,riden_scpi,modbus_bridge,rpc_bind_server,vxi_server;
struct { unsigned calls=0;void update() { ++calls; } } MDNS;
struct { unsigned calls=0;void handle() { ++calls; } } ArduinoOTA;
static bool has_time=true, did_update_time=false;
#define LOG_LN(...)

// PRODUCTION_MAIN_LOOP

static void reset() {
    firmware_update_state()={};Update={};ESP={};WiFiUDP::stops=0;riden_config={};
    http_server.client_state={};http_server.server.status=0;
    http_server.server.body.clear();http_server.server.connection.clear();http_server.server.portal="false";
    http_server.server.content_length=0;http_server.on_poll=nullptr;http_server.polls=0;
    clock_calls=MDNS.calls=ArduinoOTA.calls=0;
    has_time=true;did_update_time=false;
    riden_modbus.calls=riden_scpi.calls=modbus_bridge.calls=rpc_bind_server.calls=vxi_server.calls=0;
}
static void upload(UploadStatus status,size_t bytes=0) {
    http_server.server.file.status=status;
    http_server.server.file.currentSize=bytes;
    http_server.handle_firmware_update_post();
}
static void complete_upload() {
    upload(UPLOAD_FILE_START);upload(UPLOAD_FILE_WRITE,16);upload(UPLOAD_FILE_END);
    http_server.finish_firmware_update_post();
}
static void failed_without_restart() {
    assert(http_server.server.status==500 && !firmware_update_state().restart_pending);
    assert(ESP.restarts==0 && !firmware_update_state().success);
}

int main() {
    reset();http_server.finish_firmware_update_post();failed_without_restart();
    std::cout<<"PASS: an empty firmware POST cannot report success or schedule a reboot\n";

    reset();Update.begin_ok=false;complete_upload();failed_without_restart();
    assert(Update.finishes==0);
    std::cout<<"PASS: updater begin failure is retained even without an updater error code\n";

    reset();Update.end_ok=false;complete_upload();failed_without_restart();assert(Update.finishes==1);
    std::cout<<"PASS: Update.end(false result) without an error code cannot report success\n";

    reset();Update.partial=true;complete_upload();failed_without_restart();
    assert(firmware_update_state().received_bytes==16 && firmware_update_state().written_bytes==15);
    assert(Update.finishes==0);
    std::cout<<"PASS: a partial flash write prevents success and reboot\n";

    reset();upload(UPLOAD_FILE_START);upload(UPLOAD_FILE_WRITE,16);upload(UPLOAD_FILE_ABORTED);
    http_server.finish_firmware_update_post();failed_without_restart();
    std::cout<<"PASS: an aborted firmware upload cannot report success or reboot\n";

    reset();complete_upload();
    const auto &state=firmware_update_state();
    assert(http_server.server.status==200 && state.success && state.finished && state.restart_pending);
    assert(http_server.server.connection=="close");
    assert(http_server.server.content_length==http_server.server.body.size());
    assert(ESP.restarts==0 && WiFiUDP::stops==0);
    loop();assert(ESP.restarts==0 && clock_calls==0 && riden_modbus.calls==0 && http_server.polls==0);
    delay(500);loop();
    assert(ESP.restarts==1 && WiFiUDP::stops==1 && http_server.client_state.stop_timeout==200);
    assert(clock_calls==0 && riden_modbus.calls==0 && ArduinoOTA.calls==0);
    std::cout<<"PASS: successful OTA responds completely then restarts from loop with no UART or clock work\n";

    reset();complete_upload();http_server.client_state.stop_ok=false;delay(500);loop();
    assert(ESP.restarts==1 && http_server.client_state.aborts==1);
    assert(http_server.client_state.stop_timeout==200);
    std::cout<<"PASS: an unacknowledged HTTP connection is aborted after a bounded stop before reboot\n";

    reset();http_server.server.portal="true";http_server.handle_reboot_dongle_get();
    assert(riden_config.portals==1 && riden_config.commits==1 && firmware_update_state().restart_pending);
    assert(Update.begins==0 && ESP.restarts==0);delay(500);loop();assert(ESP.restarts==1);
    std::cout<<"PASS: manual/config-portal reboot uses the same deferred restart path\n";

    reset();http_server.request_restart();const auto at=firmware_update_state().restart_requested_ms;
    delay(200);http_server.request_restart();assert(firmware_update_state().restart_requested_ms==at);
    delay(300);loop();assert(ESP.restarts==1);
    std::cout<<"PASS: repeated restart requests cannot postpone the original deadline\n";

    reset();loop();assert(clock_calls==1 && riden_modbus.calls==1 && http_server.polls==1 && ArduinoOTA.calls==1);
    http_server.on_poll=[] { http_server.request_restart(); };
    loop();assert(ArduinoOTA.calls==1);const auto calls=clock_calls;
    const auto uart_calls=riden_modbus.calls;
    loop();assert(clock_calls==calls && riden_modbus.calls==uart_calls);
    std::cout<<"PASS: normal loop is unchanged; a newly pending restart skips ArduinoOTA and subsequent polling\n";

    reset();host_time_us=((uint64_t(1)<<32)-200)*1000;
    http_server.request_restart();delay(400);loop();assert(ESP.restarts==0);
    delay(101);loop();assert(ESP.restarts==1);
    std::cout<<"PASS: deferred restart deadline survives millis rollover\n";
}
