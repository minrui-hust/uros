#pragma once

#include "etl/vector.h"

#include "uros_config.h"

#include "topic.h"
#include "transport.h"

namespace uros {

struct TransportRemote : public TransportBase {
  void init() override;

protected:
  // clang-format off
  bool routeInNormal(const MsgBase *msg, int from_tsp, int timeout_ms) override;
  bool routeInRequest(const MsgBase *msg, int from_tsp, int timeout_ms) override;
  bool routeInResponse(const MsgBase *msg, int from_tsp, int timeout_ms) override;
  bool routeInServiceBroadcast(const MsgBase *msg, int from_tsp, int timeout_ms) override;
  bool routeInServiceDiscovery(const MsgBase *msg, int from_tsp, int timeout_ms) override;
  // clang-format on

  void recvNormal(const MsgBase *msg);
  void recvRequest(const MsgBase *msg);
  void recvResponse(const MsgBase *msg);
  void recvServiceBroadcast(const MsgBase *msg);
  void recvServiceDiscovery(const MsgBase *msg);

  // thread safe
  virtual int send(const void *data, size_t len, int prio,
                   int timeout_ms = -1) = 0;

  // thread safe
  virtual int recv(void *data, size_t len, int *prio = nullptr,
                   int timeout_ms = -1) = 0;

  void sendWork();

  void recvWork();

protected:
  union Buffer {
    MsgBase msg;
    uint8_t data[UROS_MSG_MAX_SIZE];
  };

  std::unique_ptr<Thread> send_worker_;

  std::unique_ptr<Thread> recv_worker_;
  Buffer recv_buf_;
};

} // namespace uros
