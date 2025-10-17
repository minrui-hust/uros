#pragma once

#include "topic.h"
#include "transport.h"

namespace uros {

struct TransportLocal : public TransportBase {

  // interface with publisher
  template <typename Topic>
  int write(Topic *topic, const typename Topic::Msg &msg);

  template <typename Topic> void notify(Topic *topic);

  // interface with client
  template <typename Service>
  bool call(Service *service, const typename Service::Req &req,
            typename Service::Rsp &rsp, int timeout_ms);

protected:
  bool routeInNormal(const MsgBase *msg, int from_tsp, int timeout_ms) override;
  bool routeInRequest(const MsgBase *msg, int from_tsp,
                      int timeout_ms) override;
  bool routeInResponse(const MsgBase *msg, int from_tsp,
                       int timeout_ms) override;
  bool routeInServiceBroadcast(const MsgBase *msg, int from_tsp,
                               int timeout_ms) override;
  bool routeInServiceDiscovery(const MsgBase *msg, int from_tsp,
                               int timeout_ms) override;

  template <typename Service>
  bool remoteCall(Service *service, const typename Service::Req &req,
                  typename Service::Rsp &rsp, int timeout_ms);
};

} // namespace uros
