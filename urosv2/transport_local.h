#pragma once

#include "topic.h"
#include "transport.h"

namespace uros {

struct TransportLocal : public TransportBase {

  bool put(MsgBase *msg, int from_tsp, int timeout_ms) override;

  template <typename Topic>
  void sendMsg(Topic *topic, const typename Topic::Msg &msg);

  template <typename Service>
  void sendReq(Service *service, const typename Service::Req &req);

  template <typename Service>
  void sendRsp(Service *service, const typename Service::Rsp &rsp);

protected:
  bool putNormal(MsgBase *msg, int from_tsp, int timeout_ms);
  bool putRequest(MsgBase *msg, int from_tsp, int timeout_ms);
  bool putResponse(MsgBase *msg, int from_tsp, int timeout_ms);
  bool putServiceBroadcast(MsgBase *msg, int from_tsp, int timeout_ms);
  bool putServiceDiscovery(MsgBase *msg, int from_tsp, int timeout_ms);
};

} // namespace uros
