#pragma once

#include "transport_uart.h"

namespace uros {

inline void TransportUart::initUart(const char *uart_name) {

  // 打开设备
  uart_ = Device::open<Uart>(uart_name);
}

inline int TransportUart::send(const void *data, size_t len, int prio,
                               int timeout_ms) {
  UROS_PRINT("transport_uart.send: %zu\n", len);

  // 封包

  return uart_->send((const char *)data, len, timeout_ms);
}

inline int TransportUart::recv(void *data, size_t len, int *prio,
                               int timeout_ms) {

  int received = uart_->recv((char *)data, len, timeout_ms);

  // 解包

  UROS_PRINT("transport_uart.recv: %d\n", received);
  return received;
}

} // namespace uros
