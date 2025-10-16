#pragma once

#include "transport_remote_uart.h"

namespace uros {

inline void TransportRemoteUart::initUart(const char *uart_name) {

  //打开设备
  uart_ = Device::open<Uart>(uart_name);
}

inline int TransportRemoteUart::send(const void *data, size_t len, int prio,
                                     int timeout_ms) {
  UROS_PRINT("transport_remote_socket.send: %zu\n", len);

  //封包

  return uart_->send((const char *)data, len, timeout_ms);
}

inline int TransportRemoteUart::recv(void *data, size_t len, int *prio,
                                     int timeout_ms) {

  int received = uart_->recv((char *)data, len, timeout_ms);

  //解包

  UROS_PRINT("transport_remote_socket.recv: %d\n", received);
  return received;
}

} // namespace uros
