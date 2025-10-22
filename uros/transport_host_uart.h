#pragma once
#include <unistd.h>

#include "packer.h"
#include "transport.h"

namespace uros {

struct TransportHostUart : public TransportBase {

  void initHostUart(const char *drv_name);

  int send(const void *data, size_t len, int prio, int timeout_ms) override;

  // blocking recv, thread safe is NOT required
  // UDP recvfrom always receives one complete datagram (packet)
  // Returns the number of bytes in the received packet, or -1 on error
  int recv(void *data, size_t len, int *prio, int timeout_ms) override;

  ~TransportHostUart() { close(fd_); }

private:
  int fd_ = 0;
  static const uint16_t MAX_SIZE = 512;
  uint8_t recv_buf_[MAX_SIZE] = {0};
  uint8_t send_buf_[MAX_SIZE] = {0};

  PacketParser packer_;
};

} // namespace uros
