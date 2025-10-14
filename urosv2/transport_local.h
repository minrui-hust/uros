#pragma once

#include "topic.h"
#include "transport.h"

namespace uros {

struct TransportLocal : public TransportBase {

  template <typename Topic>
  void sendMsg(Topic *topic, const typename Topic::Msg &msg);

  template <typename Service>
  void sendReq(Service *service, const typename Service::Req &req);

  template <typename Service>
  void sendRsp(Service *service, const typename Service::Rsp &rsp);

protected:
  bool putNormal(const MsgBase *msg, int from_tsp, int timeout_ms) override;
  bool putRequest(const MsgBase *msg, int from_tsp, int timeout_ms) override;
  bool putResponse(const MsgBase *msg, int from_tsp, int timeout_ms) override;
  bool putServiceBroadcast(const MsgBase *msg, int from_tsp,
                           int timeout_ms) override;
  bool putServiceDiscovery(const MsgBase *msg, int from_tsp,
                           int timeout_ms) override;
};

} // namespace uros
