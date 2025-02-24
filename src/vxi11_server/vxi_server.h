#pragma once

#include "utilities.h"
#include "wifi_ext.h"
#include <ESP8266WiFi.h>
#include <SCPI_Parser.h>
#include <list>

/*!
  @brief  Interface with the rest of the device.
*/
class SCPI_handler_interface
{
  public:
    virtual ~SCPI_handler_interface() {} 
    // write a command to the SCPI parser
    virtual void write(const char *data, size_t len) = 0;
    // read a response from the SCPI parser
    virtual scpi_result_t read(char *data, size_t *len, size_t max_len) = 0;
    // claim_control() should return true if the SCPI parser is ready to accept a command
    virtual bool claim_control() = 0;
    // release_control() should be called when the SCPI parser is no longer needed
    virtual void release_control() = 0;
};

/*!
  @brief  Listens for and responds to VXI-11 requests.
*/
class VXI_Server
{

  public:
    enum Read_Type {
        rt_none = 0,
        rt_identification = 1,
        rt_parameters = 2
    };

  public:
    VXI_Server(SCPI_handler_interface &scpi_handler);
    VXI_Server(SCPI_handler_interface &scpi_handler, uint32_t port_min, uint32_t port_max);
    ~VXI_Server();

    void loop();
    void begin(bool bNext = false);
    void begin_next() { begin(true); }
    bool available() { return (!client); }
    uint32_t allocate();
    uint32_t port() { return vxi_port; }
    const char *get_visa_resource();
    std::list<IPAddress> get_connected_clients();
    void disconnect_client(const IPAddress &ip);

  protected:
    void create_link();
    void destroy_link();
    void read();
    void write();
    bool handle_packet();
    void parse_scpi(char *buffer);

    WiFiServer_ext tcp_server;
    WiFiClient client;
    Read_Type read_type;
    uint32_t rw_channel;
    cyclic_uint32_t vxi_port;
    SCPI_handler_interface &scpi_handler;
};

