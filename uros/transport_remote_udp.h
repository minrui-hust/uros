#pragma once

#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "transport_remote.h"

namespace uros {

struct TransportRemoteUdp : public TransportRemote<TransportRemoteUdp> {
  using Base = TransportRemote<TransportRemoteUdp>;

  void initSocket(const char *mcast_ip, uint16_t mcast_port) {
    printf("[TransportRemoteUdp] Initializing: %s:%d\n", mcast_ip, mcast_port);
    
    // 创建socket
    sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    CHECK(sockfd_ > 0)
    printf("[TransportRemoteUdp] Socket created: fd=%d\n", sockfd_);

    int res;

    // 设置选项
    int reuse = 1;
    res = setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    CHECK(res >= 0)
    printf("[TransportRemoteUdp] SO_REUSEADDR set\n");

    // 禁用回环 (不接收自己的消息)
    int loop = 1;
    res =
        setsockopt(sockfd_, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
    CHECK(res >= 0)
    printf("[TransportRemoteUdp] IP_MULTICAST_LOOP = %d\n", loop);

    // 绑定
    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(mcast_port);
    res = bind(sockfd_, (struct sockaddr *)&local_addr, sizeof(local_addr));
    CHECK(res >= 0)
    printf("[TransportRemoteUdp] Bound to 0.0.0.0:%d\n", mcast_port);

    // 加入组播组
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(mcast_ip);
    mreq.imr_interface.s_addr = INADDR_ANY;
    res =
        setsockopt(sockfd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    CHECK(res >= 0)
    printf("[TransportRemoteUdp] Joined multicast group %s\n", mcast_ip);

    // 配置发送地址
    memset(&multicast_addr_, 0, sizeof(multicast_addr_));
    multicast_addr_.sin_family = AF_INET;
    multicast_addr_.sin_addr.s_addr = inet_addr(mcast_ip);
    multicast_addr_.sin_port = htons(mcast_port);
    
    printf("[TransportRemoteUdp] Initialization complete\n");
  }

  int sendImpl(const void *data, size_t len) {
    printf("TransportRemoteUdp send: %d\n", (int)len);
    return sendto(sockfd_, data, len, 0, (struct sockaddr *)&multicast_addr_,
                  sizeof(multicast_addr_));
  }

  // blocking recv, thread safe is NOT required
  // UDP recvfrom always receives one complete datagram (packet)
  // Returns the number of bytes in the received packet, or -1 on error
  int recvImpl(void *data, size_t len) {
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    
    auto recv_len = recvfrom(sockfd_, data, len, 0, 
                             (struct sockaddr*)&sender_addr, &addr_len);
    
    if (recv_len > 0) {
        char sender_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip, sizeof(sender_ip));
        printf("TransportRemoteUdp recv: %d bytes from %s:%d\n", 
               (int)recv_len, sender_ip, ntohs(sender_addr.sin_port));
    }
    
    return recv_len;
  }

  ~TransportRemoteUdp() {
    if (sockfd_ >= 0) {
      close(sockfd_);
    }
  }

private:
  int sockfd_ = -1;
  struct sockaddr_in multicast_addr_;
};

} // namespace uros
