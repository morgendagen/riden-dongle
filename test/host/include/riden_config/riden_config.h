#pragma once
namespace RidenDongle {
struct HostConfig { unsigned long get_uart_baudrate() const { return 115200; } };
inline HostConfig riden_config;
}
