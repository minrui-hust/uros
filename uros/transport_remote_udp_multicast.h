#pragma once

#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "transport_remote.h"

namespace uros {

struct TransportRemoteUdpMulticast
    : public TransportRemote<TransportRemoteUdpMulticast> {
  using Base = TransportRemote<TransportRemoteUdpMulticast>;

  void initSocket(const char *mcast_ip, uint16_t mcast_port,
                  uint16_t send_port = 0) {
    UROS_PRINT("[TransportRemoteUdp] Initializing: %s:%d (send_port=%d)\n",
               mcast_ip, mcast_port, send_port);

    // ========== 1. 创建并配置接收 socket ==========
    recvfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    CHECK(recvfd_ > 0)
    UROS_PRINT("[TransportRemoteUdp] Recv socket created: fd=%d\n", recvfd_);

    int res;

    // 设置 SO_REUSEADDR 允许多个进程绑定同一端口
    int reuse = 1;
    res = setsockopt(recvfd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    CHECK(res >= 0)
    UROS_PRINT("[TransportRemoteUdp] Recv socket SO_REUSEADDR set\n");

    // 绑定到组播端口
    struct sockaddr_in recv_addr;
    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_addr.s_addr = INADDR_ANY;
    recv_addr.sin_port = htons(mcast_port);
    res = bind(recvfd_, (struct sockaddr *)&recv_addr, sizeof(recv_addr));
    CHECK(res >= 0)
    UROS_PRINT("[TransportRemoteUdp] Recv socket bound to 0.0.0.0:%d\n",
               mcast_port);

    // 加入组播组
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(mcast_ip);
    mreq.imr_interface.s_addr = INADDR_ANY;
    res =
        setsockopt(recvfd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    CHECK(res >= 0)
    UROS_PRINT("[TransportRemoteUdp] Joined multicast group %s\n", mcast_ip);

    // ========== 2. 创建并配置发送 socket ==========
    sendfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    CHECK(sendfd_ > 0)
    UROS_PRINT("[TransportRemoteUdp] Send socket created: fd=%d\n", sendfd_);

    // 绑定发送端口（固定端口或自动分配）
    struct sockaddr_in send_addr;
    memset(&send_addr, 0, sizeof(send_addr));
    send_addr.sin_family = AF_INET;
    send_addr.sin_addr.s_addr = INADDR_ANY;
    send_addr.sin_port = htons(send_port); // send_port=0 表示自动分配
    res = bind(sendfd_, (struct sockaddr *)&send_addr, sizeof(send_addr));
    CHECK(res >= 0)

    // 获取实际绑定的端口号
    socklen_t addr_len = sizeof(send_addr);
    res = getsockname(sendfd_, (struct sockaddr *)&send_addr, &addr_len);
    CHECK(res >= 0)
    my_port_ = ntohs(send_addr.sin_port);
    UROS_PRINT("[TransportRemoteUdp] Send socket bound to port %d\n", my_port_);

    // 设置组播回环（同机测试时需要）
    u_char loop = 1;
    res =
        setsockopt(sendfd_, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
    CHECK(res >= 0)
    UROS_PRINT("[TransportRemoteUdp] IP_MULTICAST_LOOP enabled\n");

    // ========== 3. 配置组播目标地址 ==========
    memset(&multicast_addr_, 0, sizeof(multicast_addr_));
    multicast_addr_.sin_family = AF_INET;
    multicast_addr_.sin_addr.s_addr = inet_addr(mcast_ip);
    multicast_addr_.sin_port = htons(mcast_port);

    UROS_PRINT("[TransportRemoteUdp] Initialization complete (my_port=%d)\n",
               my_port_);
  }

  int sendImpl(const void *data, size_t len) {
    UROS_PRINT("TransportRemoteUdp send: %d bytes from port %d\n", (int)len,
               my_port_);
    return sendto(sendfd_, data, len, 0, (struct sockaddr *)&multicast_addr_,
                  sizeof(multicast_addr_));
  }

  // blocking recv, thread safe is NOT required
  // UDP recvfrom always receives one complete datagram (packet)
  // Returns the number of bytes in the received packet, or -1 on error
  int recvImpl(void *data, size_t len) {
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);

    while (true) {
      auto recv_len = recvfrom(recvfd_, data, len, 0,
                               (struct sockaddr *)&sender_addr, &addr_len);
      if (recv_len < 0) {
        continue;
      }

      // 过滤自己发送的消息
      auto sender_port = ntohs(sender_addr.sin_port);
      if (sender_port == my_port_) {
        continue;
      }

      UROS_PRINT("TransportRemoteUdp recv: %d bytes from port %d\n",
                 (int)recv_len, sender_port);
      return recv_len;
    }
  }

  ~TransportRemoteUdpMulticast() {
    if (sendfd_ >= 0) {
      close(sendfd_);
    }
    if (recvfd_ >= 0) {
      close(recvfd_);
    }
  }

private:
  int sendfd_ = -1;
  int recvfd_ = -1;
  uint16_t my_port_ = 0;
  struct sockaddr_in multicast_addr_;
};

} // namespace uros
