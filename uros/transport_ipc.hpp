#pragma once

#include "transport_ipc.h"

namespace uros {

inline void TransportIpc::initIpc(const char *ipc_name) {

  //打开can设备
  ipc_ = Device::open<Ipc>(ipc_name);
}

inline int TransportIpc::send(const void *data, size_t len, int prio,
                              int timeout_ms) {

  return ipc_->send((const char *)data, len, 0, timeout_ms);
}

inline int TransportIpc::recv(void *data, size_t len, int *prio,
                              int timeout_ms) {
  int received = ipc_->recv((char *)data, len, 0, timeout_ms);

  UROS_PRINT("transport_remote_socket.recv: %d\n", received);
  return received;
}

} // namespace uros
