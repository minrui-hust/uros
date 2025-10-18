#pragma once

#include "can_v2.h"

#include "transport.h"

namespace uros {

struct TransportCan : public TransportBase {

  void initCan(const char *can_name, int can_id);

  int send(const void *data, size_t len, int prio, int timeout_ms) override;

  // blocking recv, thread safe is NOT required
  // UDP recvfrom always receives one complete datagram (packet)
  // Returns the number of bytes in the received packet, or -1 on error
  int recv(void *data, size_t len, int *prio, int timeout_ms) override;

  ~TransportCan() {}

private:
  CanV2 *can_;
};

} // namespace uros
