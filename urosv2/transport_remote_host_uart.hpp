#pragma once

#include "transport_remote_host_uart.h"

namespace uros {

inline void TransportRemoteHostUart::initHostUart() {

  //打开设备
}

inline int TransportRemoteHostUart::send(const void *data, size_t len, int prio,
                                         int timeout_ms) {
  UROS_PRINT("transport_remote_socket.send: %zu\n", len);

  return uart_->send((void *)data, len, prio, timeout_ms);
}

inline int TransportRemoteHostUart::recv(void *data, size_t len, int *prio,
                                         int timeout_ms) {

  int received = uart_->recv(&can_msg, 0, timeout_ms);

  UROS_PRINT("transport_remote_socket.recv: %d\n", received);
  return received;
}

} // namespace uros
