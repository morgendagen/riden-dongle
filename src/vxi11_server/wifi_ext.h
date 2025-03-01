#pragma once

/*!
  @file   wifi_ext.h
  @brief  Declaration and definition of WiFiServer_ext class.
*/

#include <ESP8266WiFi.h>

/*!
  @brief  Minor extension of WiFiServer class to allow initialization
          without a port.

  As of 10/5/2024, the latest version of the ESP8266 board installation
  (3.1.2) includes a version of WiFiServer that cannot be initialized
  without a port. However,there is a newer version of the WiFiServer library
  at https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WiFi
  which includes a default value for the port (23), allowing one to declare
  a WiFiServer variable without specifying the port. We need the functionality
  of this later version, but since it is not (yet) included by the esp8266
  board package, we must create this functionality using the WiFiServer_ext
  class. Note that the port can be changed as needed via the begin() command.
*/
class WiFiServer_ext : public WiFiServer {

  public:

  /*!
    @brief  Allow construction without parameters.

    The constructor defaults to port 23, the same
    default port as newer versions of WiFiServer.
  */
    WiFiServer_ext ()
      : WiFiServer(23)
      {}
};

