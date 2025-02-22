#pragma once

/*!
  @file   rpc_packets.h
  @brief  Declaration of data structures and basic functions to
          receive and send RPC/VXI packets.
*/

#include "utilities.h"
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

/*  The get functions take the connection (UDP or TCP client),
    read the available data, and return the length of data
    received and stored in the data_buffer.
*/

uint32_t get_bind_packet(WiFiUDP &udp);
uint32_t get_bind_packet(WiFiClient &tcp);
uint32_t get_vxi_packet(WiFiClient &tcp);

/*  The send functions take the connection (UDP or TCP client)
    and the length of the data to send; they send the data
    and return void.
*/

void send_bind_packet(WiFiUDP &udp, uint32_t len);
void send_bind_packet(WiFiClient &tcp, uint32_t len);
void send_vxi_packet(WiFiClient &tcp, uint32_t len);

/*  The send functions call on fill_response_header to generate
    the "generic" data used in all responses.
*/

void fill_response_header(uint8_t *buffer, uint32_t xid);

/*!
  @brief  Enumeration of the sizes of the various packet buffers.

  The buffers must allow sufficient space to receive the longest
  expected data for the type of packet involved.
*/
enum packet_buffer_sizes {
    UDP_READ_SIZE = 64,  ///< The UDP bind request should be 56 bytes
    UDP_SEND_SIZE = 32,  ///< The UDP bind response should be 28 bytes
    TCP_READ_SIZE = 64,  ///< The TCP bind request should be 56 bytes + 4 bytes for prefix
    TCP_SEND_SIZE = 32,  ///< The TCP bind response should be 28 bytes + 4 bytes for prefix
    VXI_READ_SIZE = 256, ///< The VXI requests should never exceed 128 bytes, but extra allowed
    VXI_SEND_SIZE = 256  ///< The VXI responses should never exceed 128 bytes, but extra allowed
};

/*  declaration of data buffers  */

extern uint8_t udp_read_buffer[]; ///< Buffer used to receive bind requests via UDP
extern uint8_t udp_send_buffer[]; ///< Buffer used to send bind responses via UDP
extern uint8_t tcp_read_buffer[]; ///< Buffer used to receive bind requests via tcp
extern uint8_t tcp_send_buffer[]; ///< Buffer used to send bind responses via tcp
extern uint8_t vxi_read_buffer[]; ///< Buffer used to receive vxi commands
extern uint8_t vxi_send_buffer[]; ///< Buffer used to send vxi responses

/*  Constants to allow access to the portions of the data_buffers
    that represent prefix or packet data for UDP and TCP communication.
*/

uint8_t *const udp_request_packet_buffer = udp_read_buffer;  ///< The packet portion of a udp bind request
uint8_t *const udp_response_packet_buffer = udp_send_buffer; ///< The packet portion of a udp bind response

uint8_t *const tcp_request_prefix_buffer = tcp_read_buffer;     ///< The prefix portion of a tcp bind request
uint8_t *const tcp_request_packet_buffer = tcp_read_buffer + 4; ///< The packet portion of a tcp bind request

uint8_t *const tcp_response_prefix_buffer = tcp_send_buffer;     ///< The prefix portion of a tcp bind response
uint8_t *const tcp_response_packet_buffer = tcp_send_buffer + 4; ///< The packet portion of a tcp bind response

uint8_t *const vxi_request_prefix_buffer = vxi_read_buffer;     ///< The prefix portion of a vxi command request
uint8_t *const vxi_request_packet_buffer = vxi_read_buffer + 4; ///< The packet portion of a vxi command request

uint8_t *const vxi_response_prefix_buffer = vxi_send_buffer;     ///< The prefix portion of a vxi command response
uint8_t *const vxi_response_packet_buffer = vxi_send_buffer + 4; ///< The packet portion of a vxi command response

/*  Structures to allow description of / access to the data buffers
    according to the type of packet. Note that any 32-bit (i.e., non-
    character) data is sent and received in big-end format. The
    structures below define the data using the big_endian_32_t class,
    which "automatically" handles conversion to/from the C++ little-end
    uint32_t type. See the rpc_enums.h file for enumeration of the various
    message, status, error, and other codes.

    Helpful information on the structure of the basic RPC packet is available
    at https://www.ibm.com/docs/it/aix/7.2?topic=concepts-remote-procedure-call.
    For information on the VXI specific packets, see the VXIbus TCP/IP Instrument
    Protocol Specification at https://vxibus.org/specifications.html.
*/

/*!
  @brief  Structure of an RCP/VXI packet prefix

  TCP communication uses a 4-byte prefix followed by the variable-
  length packet. The prefix contains the packet length (not including
  the length of the prefix) in the lower 31 bits. The most significant
  bit is a flag signalling whether there is more data to come
  ("fragment" bit).
*/
struct tcp_prefix_packet {
    big_endian_32_t length; ///< For tcp packets, this prefix contains a FRAG bit (0x80000000) and the length of the following packet
};

/*!
  @brief  Structure of the basic RPC/VXI request packet.

  All RPC/VXI requests will start with the data described
  by this structure. Depending on the type of request, there
  may be additional data as well.
*/
struct rpc_request_packet {
    big_endian_32_t xid;             ///< Transaction id (should be checked to make sure it matches, but we will just pass it back)
    big_endian_32_t msg_type;        ///< Message type (see rpc::msg_type)
    big_endian_32_t rpc_version;     ///< RPC protocol version (should be 2, but we can ignore)
    big_endian_32_t program;         ///< Program code (see rpc::programs)
    big_endian_32_t program_version; ///< Program version - what version of the program is requested (we can ignore)
    big_endian_32_t procedure;       ///< Procedure code (see rpc::procedures)
    big_endian_32_t credentials_l;   ///< Security data (not used in this context)
    big_endian_32_t credentials_h;   ///< Security data (not used in this context)
    big_endian_32_t verifier_l;      ///< Security data (not used in this context)
    big_endian_32_t verifier_h;      ///< Security data (not used in this context)
};

/*!
  @brief  Structure of the minimum RPC response packet.

  All RPC/VXI responses will start with the data described
  by this structure. Depending on the type of response, there
  may be additional data as well.
*/
struct rpc_response_packet {
    big_endian_32_t xid;         ///< Transaction id (we just pass it back what we received in the request)
    big_endian_32_t msg_type;    ///< Message type (see rpc::msg_type)
    big_endian_32_t reply_state; ///< Accepted or rejected (see rpc::reply_state)
    big_endian_32_t verifier_l;  ///< Security data (not used in this context)
    big_endian_32_t verifier_h;  ///< Security data (not used in this context)
    big_endian_32_t rpc_status;  ///< Status of accepted message (see rpc::rpc_status)
};

/*!
  @brief  Structure of the RPC bind request packet.

  When the RPC packet is a bind request, it will include additional
  data beyond the basic RPC/VXI request packet structure ... none of
  which we will actually process.
*/
struct bind_request_packet {
    big_endian_32_t xid;              ///< Transaction id (should be checked to make sure it matches, but we will just pass it back)
    big_endian_32_t msg_type;         ///< Message type (see rpc::msg_type)
    big_endian_32_t rpc_version;      ///< RPC protocol version (should be 2, but we can ignore)
    big_endian_32_t program;          ///< Program code (see rpc::programs)
    big_endian_32_t program_version;  ///< Program version - what version of the program is requested (we can ignore)
    big_endian_32_t procedure;        ///< Procedure code (see rpc::procedures)
    big_endian_32_t credentials_l;    ///< Security data (not used in this context)
    big_endian_32_t credentials_h;    ///< Security data (not used in this context)
    big_endian_32_t verifier_l;       ///< Security data (not used in this context)
    big_endian_32_t verifier_h;       ///< Security data (not used in this context)
    big_endian_32_t getport_program;  ///< We can ignore this
    big_endian_32_t getport_version;  ///< We can ignore this
    big_endian_32_t getport_protocol; ///< We can ignore this
    big_endian_32_t getport_port;     ///< we can ignore this
};

/*!
  @brief  Structure of the RPC bind response packet.

  When a bind request is received, the response should consist of
  the basic RPC/VXI request packet structure plus one more field
  containing the port number on which the VXI_Server is currently
  listening.
*/
struct bind_response_packet {
    big_endian_32_t xid;         ///< Transaction id (we just pass it back what we received in the request)
    big_endian_32_t msg_type;    ///< Message type (see rpc::msg_type)
    big_endian_32_t reply_state; ///< Accepted or rejected (see rpc::reply_state)
    big_endian_32_t verifier_l;  ///< Security data (not used in this context)
    big_endian_32_t verifier_h;  ///< Security data (not used in this context)
    big_endian_32_t rpc_status;  ///< Status of accepted message (see rpc::rpc_status)
    big_endian_32_t vxi_port;    ///< The port on which the VXI_Server is currently listening
};

/*!
  @brief  Structure of the VXI_11_CREATE_LINK request packet.

  In addition to the basic RPC request data, the CREATE_LINK request
  includes a client id (optional), lock request, and instrument name.
*/
struct create_request_packet {
    big_endian_32_t xid;             ///< Transaction id (should be checked to make sure it matches, but we will just pass it back)
    big_endian_32_t msg_type;        ///< Message type (see rpc::msg_type)
    big_endian_32_t rpc_version;     ///< RPC protocol version (should be 2, but we can ignore)
    big_endian_32_t program;         ///< Program code (see rpc::programs)
    big_endian_32_t program_version; ///< Program version - what version of the program is requested (we can ignore)
    big_endian_32_t procedure;       ///< Procedure code (see rpc::procedures)
    big_endian_32_t credentials_l;   ///< Security data (not used in this context)
    big_endian_32_t credentials_h;   ///< Security data (not used in this context)
    big_endian_32_t verifier_l;      ///< Security data (not used in this context)
    big_endian_32_t verifier_h;      ///< Security data (not used in this context)
    big_endian_32_t client_id;       ///< implementation specific id (we can ignore)
    big_endian_32_t lockDevice;      ///< request to lock device; we will ignore this
    big_endian_32_t lock_timeout;    ///< time to wait for the lock; we will ignore this
    big_endian_32_t data_len;        ///< length of the string in data field
    char data[];                     ///< name of the instrument (e.g., instr0)
};

/*!
  @brief  Structure of the VXI_11_CREATE_LINK response packet.

  In addition to the basic RPC response data, the CREATE_LINK response
  includes an error field, a link id to use throughout this link session,
  a port # that can be used to issue an abort command, and the maximum length
  of data that the instrument can receive per write request.
*/
struct create_response_packet {
    big_endian_32_t xid;              ///< Transaction id (we just pass it back what we received in the request)
    big_endian_32_t msg_type;         ///< Message type (see rpc::msg_type)
    big_endian_32_t reply_state;      ///< Accepted or rejected (see rpc::reply_state)
    big_endian_32_t verifier_l;       ///< Security data (not used in this context)
    big_endian_32_t verifier_h;       ///< Security data (not used in this context)
    big_endian_32_t rpc_status;       ///< Status of accepted message (see rpc::rpc_status)
    big_endian_32_t error;            ///< Error code (see rpc::errors)
    big_endian_32_t link_id;          ///< A unique link id to be used by subsequent calls in this session
    big_endian_32_t abort_port;       ///< Port number on which the device will listen for an asynchronous abort request
    big_endian_32_t max_receive_size; ///< maximum amount of data that can be received on each write command
};

/*!
  @brief  Structure of the VXI_11_DESTROY_LINK request packet.

  In addition to the basic RPC request data, the DESTROY_LINK request
  includes the link id.
*/
struct destroy_request_packet {
    big_endian_32_t xid;             ///< Transaction id (should be checked to make sure it matches, but we will just pass it back)
    big_endian_32_t msg_type;        ///< Message type (see rpc::msg_type)
    big_endian_32_t rpc_version;     ///< RPC protocol version (should be 2, but we can ignore)
    big_endian_32_t program;         ///< Program code (see rpc::programs)
    big_endian_32_t program_version; ///< Program version - what version of the program is requested (we can ignore)
    big_endian_32_t procedure;       ///< Procedure code (see rpc::procedures)
    big_endian_32_t credentials_l;   ///< Security data (not used in this context)
    big_endian_32_t credentials_h;   ///< Security data (not used in this context)
    big_endian_32_t verifier_l;      ///< Security data (not used in this context)
    big_endian_32_t verifier_h;      ///< Security data (not used in this context)
    big_endian_32_t link_id;         ///< Unique link id generated for this session (see CREATE_LINK)
};

/*!
  @brief  Structure of the VXI_11_DESTROY_LINK response packet.

  In addition to the basic RPC response data, the DESTROY_LINK response
  includes an error field (e.g., to signal an invalid link id).
*/
struct destroy_response_packet {
    big_endian_32_t xid;         ///< Transaction id (we just pass it back what we received in the request)
    big_endian_32_t msg_type;    ///< Message type (see rpc::msg_type)
    big_endian_32_t reply_state; ///< Accepted or rejected (see rpc::reply_state)
    big_endian_32_t verifier_l;  ///< Security data (not used in this context)
    big_endian_32_t verifier_h;  ///< Security data (not used in this context)
    big_endian_32_t rpc_status;  ///< Status of accepted message (see rpc::rpc_status)
    big_endian_32_t error;       ///< Error code (see rpc::errors)
};

/*!
  @brief  Structure of the VXI_11_DEV_READ request packet.

  In addition to the basic RPC request data, the DEV_READ request
  includes the link id, request size (maximum amount of data to send
  per response), timeouts for lock and i/o, flags (can signal the use
  of a terminating character), and the terminating character if any.
*/
struct read_request_packet {
    big_endian_32_t xid;             ///< Transaction id (should be checked to make sure it matches, but we will just pass it back)
    big_endian_32_t msg_type;        ///< Message type (see rpc::msg_type)
    big_endian_32_t rpc_version;     ///< RPC protocol version (should be 2, but we can ignore)
    big_endian_32_t program;         ///< Program code (see rpc::programs)
    big_endian_32_t program_version; ///< Program version - what version of the program is requested (we can ignore)
    big_endian_32_t procedure;       ///< Procedure code (see rpc::procedures)
    big_endian_32_t credentials_l;   ///< Security data (not used in this context)
    big_endian_32_t credentials_h;   ///< Security data (not used in this context)
    big_endian_32_t verifier_l;      ///< Security data (not used in this context)
    big_endian_32_t verifier_h;      ///< Security data (not used in this context)
    big_endian_32_t link_id;         ///< Unique link id generated for this session (see CREATE_LINK)
    big_endian_32_t request_size;    ///< Maximum amount of data requested (we will assume that we never send more than can be received))
    big_endian_32_t io_timeout;      ///< How long to wait before timing out the data request (we will ignore)
    big_endian_32_t lock_timeout;    ///< How long to wait before timing out a lock request (we will ignore)
    big_endian_32_t flags;           ///< Used to indicate whether an "end" character is supplied (we will ignore)
    char term_char;                  ///< The "end" character (we will ignore)
};

/*!
  @brief  Structure of the VXI_11_DEV_READ response packet.

  In addition to the basic RPC response data, the DEV_READ response
  includes an error field, the reason the data read ended, the
  length of data that will be returned, and the data itself.
*/
struct read_response_packet {
    big_endian_32_t xid;         ///< Transaction id (we just pass it back what we received in the request)
    big_endian_32_t msg_type;    ///< Message type (see rpc::msg_type)
    big_endian_32_t reply_state; ///< Accepted or rejected (see rpc::reply_state)
    big_endian_32_t verifier_l;  ///< Security data (not used in this context)
    big_endian_32_t verifier_h;  ///< Security data (not used in this context)
    big_endian_32_t rpc_status;  ///< Status of accepted message (see rpc::rpc_status)
    big_endian_32_t error;       ///< Error code (see rpc::errors)
    big_endian_32_t reason;      ///< Indicates why the data read ended (see rpc::reasons)
    big_endian_32_t data_len;    ///< Length of the data returned
    char data[];                 ///< The data returned
};

/*!
  @brief  Structure of the VXI_11_DEV_WRITE request packet.

  In addition to the basic RPC request data, the DEV_WRITE request
  includes the link id, timeouts for lock and i/o, flags (can signal
  the addition of an "end" character at the end of the data), length
  of the data being sent, and the data itself.
*/
struct write_request_packet {
    big_endian_32_t xid;             ///< Transaction id (should be checked to make sure it matches, but we will just pass it back)
    big_endian_32_t msg_type;        ///< Message type (see rpc::msg_type)
    big_endian_32_t rpc_version;     ///< RPC protocol version (should be 2, but we can ignore)
    big_endian_32_t program;         ///< Program code (see rpc::programs)
    big_endian_32_t program_version; ///< Program version - what version of the program is requested (we can ignore)
    big_endian_32_t procedure;       ///< Procedure code (see rpc::procedures)
    big_endian_32_t credentials_l;   ///< Security data (not used in this context)
    big_endian_32_t credentials_h;   ///< Security data (not used in this context)
    big_endian_32_t verifier_l;      ///< Security data (not used in this context)
    big_endian_32_t verifier_h;      ///< Security data (not used in this context)
    big_endian_32_t link_id;         ///< Unique link id generated for this session (see CREATE_LINK)
    big_endian_32_t io_timeout;      ///< How long to wait before timing out the data request (we will ignore)
    big_endian_32_t lock_timeout;    ///< How long to wait before timing out a lock request (we will ignore)
    big_endian_32_t flags;           ///< Used to indicate whether an "end" character is supplied (we will ignore)
    big_endian_32_t data_len;        ///< Length of the data sent
    char data[];                     ///< The data sent
};

/*!
  @brief  Structure of the VXI_11_DEV_WRITE response packet.

  In addition to the basic RPC response data, the DEV_WRITE response
  includes an error field and the length of the data that was sent.
*/
struct write_response_packet {
    big_endian_32_t xid;         ///< Transaction id (we just pass it back what we received in the request)
    big_endian_32_t msg_type;    ///< Message type (see rpc::msg_type)
    big_endian_32_t reply_state; ///< Accepted or rejected (see rpc::reply_state)
    big_endian_32_t verifier_l;  ///< Security data (not used in this context)
    big_endian_32_t verifier_h;  ///< Security data (not used in this context)
    big_endian_32_t rpc_status;  ///< Status of accepted message (see rpc::rpc_status)
    big_endian_32_t error;       ///< Error code (see rpc::errors)
    big_endian_32_t size;        ///< Number of bytes sent
};

/*  constant variables used to access the data buffers as the various structures defined above  */

rpc_request_packet *const udp_request = (rpc_request_packet *)udp_request_packet_buffer;     ///< udp_request accesses the udp_request_packet_buffer as a generic rpc request
rpc_response_packet *const udp_response = (rpc_response_packet *)udp_response_packet_buffer; ///< udp_response accesses the udp_response_packet_buffer as a generic rpc response

bind_request_packet *const udp_bind_request = (bind_request_packet *)udp_request_packet_buffer;     ///< udp_bind_request accesses the udp_request_packet_buffer as an rpc bind request
bind_response_packet *const udp_bind_response = (bind_response_packet *)udp_response_packet_buffer; ///< udp_bind_response accesses the udp_response_packet_buffer as an rpc bind response

rpc_request_packet *const tcp_request = (rpc_request_packet *)tcp_request_packet_buffer;     ///< tcp_request accesses the tcp_request_packet_buffer as a generic rpc request
rpc_response_packet *const tcp_response = (rpc_response_packet *)tcp_response_packet_buffer; ///< tcp_response accesses the tcp_response_packet_buffer as a generic rpc response

tcp_prefix_packet *const tcp_request_prefix = (tcp_prefix_packet *)tcp_request_prefix_buffer;   ///< tcp_request_prefix accesses the tcp_request_prefix_buffer as a tcp prefix
tcp_prefix_packet *const tcp_response_prefix = (tcp_prefix_packet *)tcp_response_prefix_buffer; ///< tcp_response_prefix accesses the tcp_response_prefix_buffer as a tcp prefix

bind_request_packet *const tcp_bind_request = (bind_request_packet *)tcp_request_packet_buffer;     ///< tcp_bind_request accesses the tcp_request_packet_buffer as an rpc bind request
bind_response_packet *const tcp_bind_response = (bind_response_packet *)tcp_response_packet_buffer; ///< tcp_bind_response accesses the tcp_response_packet_buffer as an rpc bind response

rpc_request_packet *const vxi_request = (rpc_request_packet *)vxi_request_packet_buffer;     ///< vxi_request accesses the vxi_request_packet_buffer as a generic rpc request
rpc_response_packet *const vxi_response = (rpc_response_packet *)vxi_response_packet_buffer; ///< vxi_response accesses the vxi_response_packet_buffer as a generic rpc response

tcp_prefix_packet *const vxi_request_prefix = (tcp_prefix_packet *)vxi_request_prefix_buffer;   ///< vxi_request_prefix accesses the vxi_request_prefix_buffer as a tcp prefix
tcp_prefix_packet *const vxi_response_prefix = (tcp_prefix_packet *)vxi_response_prefix_buffer; ///< vxi_response_prefix accesses the vxi_response_prefix_buffer as a tcp prefix

create_request_packet *const create_request = (create_request_packet *)vxi_request_packet_buffer;     ///< create_request accesses the vxi_request_packet_buffer as a create link request
create_response_packet *const create_response = (create_response_packet *)vxi_response_packet_buffer; ///< create_response accesses the vxi_response_packet_buffer as a create link response

destroy_request_packet *const destroy_request = (destroy_request_packet *)vxi_request_packet_buffer;     ///< destroy_request accesses the vxi_request_packet_buffer as a destroy link request
destroy_response_packet *const destroy_response = (destroy_response_packet *)vxi_response_packet_buffer; ///< destroy_response accesses the vxi_response_packet_buffer as a destroy link response

read_request_packet *const read_request = (read_request_packet *)vxi_request_packet_buffer;     ///< read_request accesses the vxi_request_packet_buffer as a read request
read_response_packet *const read_response = (read_response_packet *)vxi_response_packet_buffer; ///< read_response accesses the vxi_response_packet_buffer as a read response

write_request_packet *const write_request = (write_request_packet *)vxi_request_packet_buffer;     ///< write_request accesses the vxi_request_packet_buffer as a write request
write_response_packet *const write_response = (write_response_packet *)vxi_response_packet_buffer; ///< write_response accesses the vxi_response_packet_buffer as a write response
