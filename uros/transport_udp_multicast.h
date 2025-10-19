#pragma once

#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "transport.h"

namespace uros {

struct TransportUdpMulticast : public TransportBase {

  void initSocket(const char *mcast_ip, uint16_t mcast_port,
                  uint16_t send_port = 0);

  int send(const void *data, size_t len, int prio, int timeout_ms) override;

  int recv(void *data, size_t len, int *prio, int timeout_ms) override;

  ~TransportUdpMulticast();

private:
  int sendfd_ = -1;
  int recvfd_ = -1;
  uint16_t my_port_ = 0;
  struct sockaddr_in multicast_addr_;
};

} // namespace uros
