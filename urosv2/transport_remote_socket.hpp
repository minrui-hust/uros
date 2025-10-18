#pragma once

#include "transport_remote_socket.h"

namespace uros {

inline void TransportRemoteSocket::initSocket(const char *local_addr,
                                              uint16_t local_port,
                                              const char *remote_addr,
                                              uint16_t remote_port) {
  // Create UDP socket
  sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
  assert(sockfd_ >= 0);

  // Setup local address
  struct sockaddr_in local_sockaddr;
  std::memset(&local_sockaddr, 0, sizeof(local_sockaddr));
  local_sockaddr.sin_family = AF_INET;
  local_sockaddr.sin_port = htons(local_port);
  local_sockaddr.sin_addr.s_addr =
      local_addr ? inet_addr(local_addr) : INADDR_ANY;

  // Bind to local address
  int ret =
      bind(sockfd_, (struct sockaddr *)&local_sockaddr, sizeof(local_sockaddr));
  assert(ret == 0);

  // Setup remote address
  std::memset(&remote_sockaddr_, 0, sizeof(remote_sockaddr_));
  remote_sockaddr_.sin_family = AF_INET;
  remote_sockaddr_.sin_port = htons(remote_port);
  remote_sockaddr_.sin_addr.s_addr = inet_addr(remote_addr);
}

inline int TransportRemoteSocket::send(const void *data, size_t len, int prio,
                                       int timeout_ms) {
  UROS_PRINT("transport_remote_socket.send: %zu\n", len);
  return sendto(sockfd_, data, len, 0, (struct sockaddr *)&remote_sockaddr_,
                sizeof(remote_sockaddr_));
}

inline int TransportRemoteSocket::recv(void *data, size_t len, int *prio,
                                       int timeout_ms) {
  int received = 0;
  while (received <= 0) {
    received = recvfrom(sockfd_, data, len, 0, nullptr, nullptr);
  }
  UROS_PRINT("transport_remote_socket.recv: %d\n", received);
  return received;
}

} // namespace uros
