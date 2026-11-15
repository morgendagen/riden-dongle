// SPDX-FileCopyrightText: 2024 Peder Toftegaard Olsen
//
// SPDX-License-Identifier: MIT

#include <riden_logging/riden_logging.h>
#include <riden_modbus_bridge/riden_modbus_bridge.h>

#include <ESP8266mDNS.h>
#include <algorithm>
#include <cstring>

using namespace RidenDongle;

// Callbacks within the esp8266-modbus library do
// not allow for instance methods to be used, so for
// the time being we stick to only allowing a single
// instance of RidenModbusBridge.
static RidenModbusBridge *one_and_only = nullptr;
static Modbus::ResultCode modbus_tcp_raw_callback(uint8_t *data, uint8_t len, void *custom_data);
static Modbus::ResultCode modbus_rtu_raw_callback(uint8_t *data, uint8_t len, void *custom);

bool RidenModbusBridge::begin()
{
    if (initialized) {
        return true;
    }
    if (one_and_only != nullptr) {
        return false;
    }

    LOG_LN("RidenModbusBridge initializing");

    modbus_tcp.onRaw(::modbus_tcp_raw_callback);
    modbus_tcp.server();

    if (MDNS.isRunning()) {
        // See https://github.com/espressif/esp-idf/blob/master/examples/protocols/modbus/tcp/mb_tcp_master/main/tcp_master.c#L266
        auto modbus_service = MDNS.addService(NULL, "modbus", "tcp", MODBUSTCP_PORT);
        MDNS.addServiceTxt(modbus_service, "unitid", MODBUS_ADDRESS);
    }

    LOG_LN("RidenModbusBridge initialized");

    one_and_only = this;
    initialized = true;
    return true;
}

bool RidenModbusBridge::loop()
{
    modbus_tcp.task();
    return true;
}

uint16_t RidenModbusBridge::port()
{
    return MODBUSTCP_PORT;
}

std::list<IPAddress> RidenModbusBridge::get_connected_clients()
{
    return modbus_tcp.get_connected_clients();
}

void RidenModbusBridge::disconnect_client(const IPAddress &ip)
{
    LOG_LN("RidenModbusBridge::disconnect_client");
    modbus_tcp.disconnect_client(ip);
}

/**
 * Data received from the TCP-end is forwarded to ModbusRTU,
 * which in turn forwards it to the power supply.
 */
Modbus::ResultCode RidenModbusBridge::modbus_tcp_raw_callback(uint8_t *data, uint8_t len, void *custom_data)
{
    if (!initialized) {
        return Modbus::EX_GENERAL_FAILURE;
    }
    // Wait until no transaction is active
#ifdef MOCK_RIDEN
    return Modbus::EX_SUCCESS;
#else
    const auto *source = static_cast<Modbus::frame_arg_t *>(custom_data);
    if (!source->to_server) {
        return Modbus::EX_PASSTHROUGH;
    }

    // Draining the previous UART request can send its TCP response. In
    // particular, do not use rawResponce() there: it frees task()'s receive
    // buffer, including the request pointed to by 'data'.
    if (!riden_modbus.wait_for_inactive()) {
        return Modbus::EX_SUCCESS;
    }

    transaction_id = source->transactionId;
    slave_id = source->slaveId;
    client_slot = modbus_tcp.current_client();
    request_pending = true;
    ++diagnostics.requests;
    request_length = len;
    std::memset(request_header, 0, sizeof(request_header));
    std::memcpy(request_header, data, std::min<size_t>(len, sizeof(request_header)));

    // This gateway has one PSU at address 1. A broadcast does not create an
    // RTU transaction, so installing an interceptor for it would steal the
    // next local/web response.
    if (slave_id != MODBUS_ADDRESS) {
        fail_request(Modbus::EX_PATH_UNAVAILABLE);
        return Modbus::EX_SUCCESS;
    }
    if (len == 0) {
        fail_request(Modbus::EX_ILLEGAL_VALUE);
        return Modbus::EX_SUCCESS;
    }

    const bool read_only = data[0] >= Modbus::FC_READ_COILS &&
                           data[0] <= Modbus::FC_READ_INPUT_REGS;
    if (read_only && len != 5) {
        fail_request(Modbus::EX_ILLEGAL_VALUE);
        return Modbus::EX_SUCCESS;
    }
    // Complete the UART attempt before returning from this callback. This
    // keeps a read retry in the same ownership scope as the TCP request;
    // HTTP/local work cannot slip between it and a late response.
    const uint32_t exceptions_before = diagnostics.uart_exceptions;
    for (unsigned attempt = 0; attempt < (read_only ? 2U : 1U); ++attempt) {
        if (!riden_modbus.prepare_request()) break;
        if (attempt) ++diagnostics.read_retries;
        riden_modbus.modbus.onRaw(::modbus_rtu_raw_callback);
        const bool sent = riden_modbus.modbus.rawRequest(
            slave_id, data, len,
            [this](Modbus::ResultCode code, uint16_t, void *) {
                if (request_pending) {
                    // onRaw already sent successful/exception replies. A
                    // failure is finalized or retried only AFTER task()
                    // releases its callback and outgoing RTU frame.
                    riden_modbus.modbus.onRaw(nullptr);
                    riden_modbus.uart_recovery_pending = true;
                    diagnostics.last_failed_function = request_header[0];
                    diagnostics.last_failed_register =
                        (uint16_t(request_header[1]) << 8) | request_header[2];
                    if (code == Modbus::EX_TIMEOUT) {
                        ++diagnostics.uart_timeouts;
                        diagnostics.last_failure_code = uint16_t(code);
                    }
                }
                return true;
            });
        if (!sent) {
            riden_modbus.modbus.onRaw(nullptr);
            break;
        }
        riden_modbus.wait_for_inactive();
        if (!request_pending) {
            if (attempt && diagnostics.uart_exceptions == exceptions_before)
                ++diagnostics.recovered_reads;
            return Modbus::EX_SUCCESS;
        }
    }
    if (request_pending) {
        fail_request(Modbus::EX_DEVICE_FAILED_TO_RESPOND);
    }
    return Modbus::EX_SUCCESS; // Stops ModbusTCP from processing the data
#endif
}

/**
 * Data received from the RTU-end must be forwarded to the TCP-end. Anything
 * else is passed through unaltered to ModbusRTU.
 */
Modbus::ResultCode RidenModbusBridge::modbus_rtu_raw_callback(uint8_t *data, uint8_t len, void *custom)
{
    if (!initialized) {
        return Modbus::EX_GENERAL_FAILURE;
    }
#ifdef MOCK_RIDEN
    return Modbus::EX_SUCCESS;
#else

    const Modbus::frame_arg_t *source = static_cast<Modbus::frame_arg_t *>(custom);
    if (!request_pending || source->to_server) {
        return Modbus::EX_PASSTHROUGH;
    }
    if (source->slaveId != slave_id || len == 0 ||
        (data[0] & 0x7f) != request_header[0]) {
        ++diagnostics.invalid_responses;
        diagnostics.last_failure_code = uint16_t(Modbus::EX_DATA_MISMACH);
        // Not this transaction. Keep waiting for its reply/timeout.
        return Modbus::EX_SUCCESS;
    }

    if (valid_response(data, len)) {
        if (data[0] & 0x80) ++diagnostics.uart_exceptions;
        modbus_tcp.send_response(client_slot, transaction_id, slave_id, data, len);
        finish_request();
    } else {
        // Never turn a stale 20-register web reply into a one-register TCP
        // reading. Let the caller see an error instead of plausible garbage.
        ++diagnostics.invalid_responses;
        diagnostics.last_failure_code = uint16_t(Modbus::EX_DATA_MISMACH);
        riden_modbus.uart_recovery_pending = true;
    }
    return Modbus::EX_SUCCESS; // Stops ModbusRTU from processing the data
#endif
}

void RidenModbusBridge::finish_request()
{
    riden_modbus.modbus.onRaw(nullptr);
    request_pending = false;
    transaction_id = 0;
    slave_id = 0;
    client_slot = -1;
    request_length = 0;
}

void RidenModbusBridge::fail_request(Modbus::ResultCode error)
{
    ++diagnostics.gateway_failures;
    const uint8_t response[] = {uint8_t(request_header[0] | 0x80), uint8_t(error)};
    modbus_tcp.send_response(client_slot, transaction_id, slave_id, response, sizeof(response));
    finish_request();
}

bool RidenModbusBridge::valid_response(const uint8_t *data, uint8_t len) const
{
    if (data[0] & 0x80) {
        return len == 2;
    }
    const uint16_t count = (uint16_t(request_header[3]) << 8) | request_header[4];
    switch (request_header[0]) {
    case Modbus::FC_READ_COILS:
    case Modbus::FC_READ_INPUT_STAT:
        return request_length == 5 && len >= 2 && data[1] == (count + 7) / 8 &&
               len == 2 + (count + 7) / 8;
    case Modbus::FC_READ_REGS:
    case Modbus::FC_READ_INPUT_REGS:
    case Modbus::FC_READWRITE_REGS:
        return request_length >= 5 && len >= 2 && data[1] == count * 2 && len == 2 + count * 2;
    case Modbus::FC_WRITE_COIL:
    case Modbus::FC_WRITE_REG:
    case Modbus::FC_WRITE_COILS:
    case Modbus::FC_WRITE_REGS:
        return request_length >= 5 && len == 5 && std::memcmp(data, request_header, 5) == 0;
    default:
        return true;
    }
}

bool RidenModbusTCP::send_response(int8_t client, uint16_t transaction, uint8_t unit,
                                 const uint8_t *data, uint8_t len)
{
    if (client < 0 || client >= MODBUSIP_MAX_CLIENTS ||
        !tcpclient[client] || !tcpclient[client]->connected()) {
        return false;
    }
    uint8_t frame[7 + MODBUS_MAX_FRAME];
    frame[0] = transaction >> 8;
    frame[1] = transaction & 0xff;
    frame[2] = frame[3] = 0;
    frame[4] = (uint16_t(len) + 1) >> 8;
    frame[5] = (uint16_t(len) + 1) & 0xff;
    frame[6] = unit;
    std::memcpy(frame + 7, data, len);
    // Preserve the incoming transaction ID verbatim, including zero.
    return tcpclient[client]->write(frame, size_t(len) + 7) == size_t(len) + 7;
}

Modbus::ResultCode modbus_tcp_raw_callback(uint8_t *data, uint8_t len, void *custom_data)
{
    return one_and_only->modbus_tcp_raw_callback(data, len, custom_data);
}

Modbus::ResultCode modbus_rtu_raw_callback(uint8_t *data, uint8_t len, void *custom)
{
    return one_and_only->modbus_rtu_raw_callback(data, len, custom);
}

std::list<IPAddress> RidenModbusTCP::get_connected_clients()
{
    std::list<IPAddress> connected_clients;
    for (int i = 0; i < MODBUSIP_MAX_CLIENTS; i++) {
        if (tcpclient[i] != nullptr && tcpclient[i]->connected()) {
            connected_clients.push_back(tcpclient[i]->remoteIP());
        }
    }
    return connected_clients;
}

void RidenModbusTCP::disconnect_client(const IPAddress &ip)
{
    for (int i = 0; i < MODBUSIP_MAX_CLIENTS; ++i) {
        if (tcpclient[i] && uint32_t(tcpclient[i]->remoteIP()) == uint32_t(ip)) {
            tcpclient[i]->flush();
            delete tcpclient[i];
            tcpclient[i] = nullptr;
        }
    }
}
