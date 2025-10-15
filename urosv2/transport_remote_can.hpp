#pragma once

#include "transport_remote_can.h"

namespace uros {

inline void TransportRemoteCan::initCan(const char *can_name, int can_id) {

  //打开can设备
  can_ = Device::open<CanV2>(can_name);

  //配置滤波器
  can_->addFilterWithMask(can_id, 0x7ff, 0);
}

inline int TransportRemoteCan::send(const void *data, size_t len, int prio,
                                    int timeout_ms) {
  UROS_PRINT("transport_remote_socket.send: %zu\n", len);

  return can_->send((void *)data, len, prio, timeout_ms);
}

inline int TransportRemoteCan::recv(void *data, size_t len, int *prio,
                                    int timeout_ms) {
  CanMsg can_msg = {0};

  int received = can_->recv(&can_msg, 0, timeout_ms);
  *prio = can_msg.id;
  memcpy(data, can_msg.data, can_msg.len);

  UROS_PRINT("transport_remote_socket.recv: %d\n", received);
  return received;
}

} // namespace uros
