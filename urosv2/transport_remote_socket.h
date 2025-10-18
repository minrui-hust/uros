#pragma once

#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "transport_remote.h"

namespace uros {

struct TransportRemoteSocket : public TransportRemote {

  void initSocket(const char *local_addr, uint16_t local_port,
                  const char *remote_addr, uint16_t remote_port);

  int send(const void *data, size_t len, int prio, int timeout_ms) override;

  // blocking recv, thread safe is NOT required
  // UDP recvfrom always receives one complete datagram (packet)
  // Returns the number of bytes in the received packet, or -1 on error
  int recv(void *data, size_t len, int *prio, int timeout_ms) override;

  ~TransportRemoteSocket() {
    if (sockfd_ >= 0) {
      close(sockfd_);
    }
  }

private:
  int sockfd_ = -1;
  struct sockaddr_in remote_sockaddr_;
};

} // namespace uros
