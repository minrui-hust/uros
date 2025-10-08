#pragma once

#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "transport_remote.h"

namespace uros {

struct TransportRemoteSocket : public TransportRemote<TransportRemoteSocket> {
  using Base = TransportRemote<TransportRemoteSocket>;

  void initSocket(const char *local_addr, uint16_t local_port,
                  const char *remote_addr, uint16_t remote_port) {
    // Create UDP socket
    sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    UROS_ASSERT(sockfd_ >= 0);

    // Setup local address
    struct sockaddr_in local_sockaddr;
    std::memset(&local_sockaddr, 0, sizeof(local_sockaddr));
    local_sockaddr.sin_family = AF_INET;
    local_sockaddr.sin_port = htons(local_port);
    local_sockaddr.sin_addr.s_addr =
        local_addr ? inet_addr(local_addr) : INADDR_ANY;

    // Bind to local address
    int ret = bind(sockfd_, (struct sockaddr *)&local_sockaddr,
                   sizeof(local_sockaddr));
    UROS_ASSERT(ret == 0);

    // Setup remote address
    std::memset(&remote_sockaddr_, 0, sizeof(remote_sockaddr_));
    remote_sockaddr_.sin_family = AF_INET;
    remote_sockaddr_.sin_port = htons(remote_port);
    remote_sockaddr_.sin_addr.s_addr = inet_addr(remote_addr);
  }

  int sendImpl(const void *data, size_t len) {
    UROS_PRINT("transport_udp.sendImpl: %zu\n", len);
    return sendto(sockfd_, data, len, 0, (struct sockaddr *)&remote_sockaddr_,
                  sizeof(remote_sockaddr_));
  }

  // blocking recv, thread safe is NOT required
  // UDP recvfrom always receives one complete datagram (packet)
  // Returns the number of bytes in the received packet, or -1 on error
  int recvImpl(void *data, size_t len) {
    int received = 0;
    while (received <= 0) {
      received = recvfrom(sockfd_, data, len, 0, nullptr, nullptr);
    }
    UROS_PRINT("transport_udp.recvImpl: %d\n", received);
    return received;
  }

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
