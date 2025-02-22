#pragma once

#include "utilities.h"
#include "wifi_ext.h"
#include <ESP8266WiFi.h>
#include <list>

/*!
  @brief  Interface with the rest of the device.
*/
class SCPI_handler
{
  // TODO: fill in
  public:
    SCPI_handler() {};
    ~SCPI_handler() {};
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
    VXI_Server(SCPI_handler &scpi_handler);
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
    SCPI_handler &scpi_handler;
};

