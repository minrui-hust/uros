#pragma once

#include "ipc.h"

#include "transport.h"

namespace uros {

struct TransportIpc : public TransportBase {

  void initIpc(const char *ipc_name);

  int send(const void *data, size_t len, int prio, int timeout_ms) override;

  // blocking recv, thread safe is NOT required
  // UDP recvfrom always receives one complete datagram (packet)
  // Returns the number of bytes in the received packet, or -1 on error
  int recv(void *data, size_t len, int *prio, int timeout_ms) override;

  ~TransportIpc() {}

private:
  Ipc *ipc_;
};

} // namespace uros
