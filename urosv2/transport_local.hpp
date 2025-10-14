#pragma once

#include "transport_local.h"

namespace uros {

inline bool TransportLocal::putNormal(const MsgBase *msg, int from_tsp,
                                      int timeout_ms) {
  // TODO: check for redundant
  auto topic_id = msg->__id__.entry;
  if (topic_id >= topic_metas_.size()) {
    return false;
  }

  auto &topic_meta = topic_metas_[topic_id];
  if (!topic_meta.topic) {
    return false;
  }

  topic_meta.topic->recv(msg);

  return true;
}

inline bool TransportLocal::putRequest(const MsgBase *msg, int from_tsp,
                                       int timeout_ms) {
  return false; // TODO
}

inline bool TransportLocal::putResponse(const MsgBase *msg, int from_tsp,
                                        int timeout_ms) {
  return false; // TODO
}

inline bool TransportLocal::putServiceBroadcast(const MsgBase *msg,
                                                int from_tsp, int timeout_ms) {
  return false; // TODO
}

inline bool TransportLocal::putServiceDiscovery(const MsgBase *msg,
                                                int from_tsp, int timeout_ms) {
  return false; // TODO
}

template <typename Topic>
void TransportLocal::sendMsg(Topic *topic, const typename Topic::Msg &msg) {
  msg.__id__.seq = topic->update(msg);
  router_->route(&msg, id_, -1, 0); // broadcast without timeout
}

template <typename Service>
void sendReq(Service *service, const typename Service::Req &req) {
  // TODO
}

template <typename Service>
void sendRsp(Service *service, const typename Service::Rsp &rsp) {
  // TODO
}

} // namespace uros
