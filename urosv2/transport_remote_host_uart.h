#pragma once
#include <unistd.h>

#include "transport_remote.h"

namespace uros {

struct TransportRemoteHostUart : public TransportRemote {

  void initHostUart();

  int send(const void *data, size_t len, int prio, int timeout_ms) override;

  // blocking recv, thread safe is NOT required
  // UDP recvfrom always receives one complete datagram (packet)
  // Returns the number of bytes in the received packet, or -1 on error
  int recv(void *data, size_t len, int *prio, int timeout_ms) override;

  ~TransportRemoteHostUart() { close(fd_); }

private:
  int fd_ = 0;
};

} // namespace uros
