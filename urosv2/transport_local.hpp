#pragma once

#include "transport_local.h"

namespace uros {

inline bool TransportLocal::routeInNormal(const MsgBase *msg, int from_tsp,
                                          int timeout_ms) {
  // TODO: check for redundant
  auto topic_id = msg->__meta__.id.entry;
  if (topic_id >= topic_metas_.size()) {
    return false;
  }

  auto &topic_meta = topic_metas_[topic_id];
  if (!topic_meta.topic) {
    return false;
  }

  topic_meta.topic->recvWrite(msg);

  return true;
}

inline bool TransportLocal::routeInRequest(const MsgBase *msg, int from_tsp,
                                           int timeout_ms) {
  return false; // TODO
}

inline bool TransportLocal::routeInResponse(const MsgBase *msg, int from_tsp,
                                            int timeout_ms) {
  return false; // TODO
}

inline bool TransportLocal::routeInServiceBroadcast(const MsgBase *msg,
                                                    int from_tsp,
                                                    int timeout_ms) {
  return false; // TODO
}

inline bool TransportLocal::routeInServiceDiscovery(const MsgBase *msg,
                                                    int from_tsp,
                                                    int timeout_ms) {
  return false; // TODO
}

template <typename Topic>
int TransportLocal::write(Topic *topic, const typename Topic::Msg &msg) {
  int gen = topic->doWrite(msg);
  msg.__meta__.id.seq = gen;
  router_->route(&msg, id_, -1, 0); // broadcast
  return gen;
}

template <typename Service>
bool TransportLocal::call(Service *service, const typename Service::Req &req,
                          typename Service::Rsp &rsp, int timeout_ms) {
  if (service->serverPresent()) {
    return service->doCall(req, rsp, timeout_ms);
  } else {
    return remoteCall(&req, &rsp, timeout_ms);
  }
}

inline bool TransportLocal::remoteCall(const MsgBase *req, MsgBase *rsp,
                                       int timeout_ms) {
  return false; // TODO
}

} // namespace uros
