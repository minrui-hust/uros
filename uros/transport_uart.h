#pragma once

#include "uart.h"

#include "transport.h"

namespace uros {

struct TransportUart : public TransportBase {

  void initUart(const char *uart_name);

  int send(const void *data, size_t len, int prio, int timeout_ms) override;

  // blocking recv, thread safe is NOT required
  // UDP recvfrom always receives one complete datagram (packet)
  // Returns the number of bytes in the received packet, or -1 on error
  int recv(void *data, size_t len, int *prio, int timeout_ms) override;

  ~TransportUart() {}

private:
  Uart *uart_;
};

} // namespace uros
