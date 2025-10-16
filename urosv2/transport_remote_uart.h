#pragma once

#include "uart.h"

#include "transport_remote.h"

namespace uros {

struct TransportRemoteUart : public TransportRemote {

  void initUart(const char *uart_name);

  int send(const void *data, size_t len, int prio, int timeout_ms) override;

  // blocking recv, thread safe is NOT required
  // UDP recvfrom always receives one complete datagram (packet)
  // Returns the number of bytes in the received packet, or -1 on error
  int recv(void *data, size_t len, int *prio, int timeout_ms) override;

  ~TransportRemoteUart() {}

private:
  Uart *uart_;
};

} // namespace uros
