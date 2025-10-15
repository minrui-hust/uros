#pragma once

#include "transport_remote_can.h"

namespace uros {

inline void TransportRemoteCan::initCan(const char *can_name) {

  //打开can设备
  can_ = Device::open<CanV2>("can_name");

  //配置滤波器
}

inline int TransportRemoteCan::send(const void *data, size_t len, int prio,
                                    int timeout_ms) {
  UROS_PRINT("transport_remote_socket.send: %zu\n", len);

  return can_.send(data, len, prio, timeout_ms);
}

inline int TransportRemoteCan::recv(void *data, size_t len, int *prio,
                                    int timeout_ms) {
  int received = 0;
  CanMsg can_msg = {0};

  received = can_.recv();

  UROS_PRINT("transport_remote_socket.recv: %d\n", received);
  return received;
}

} // namespace uros
