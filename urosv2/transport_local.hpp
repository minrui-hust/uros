#pragma once

#include "transport_local.h"

namespace uros {

inline bool TransportLocal::put(MsgBase *msg, int from_tsp, int timeout_ms) {
  if (msg->__id__.type == MsgTypeNormal) {
    return putNormal(msg, from_tsp, timeout_ms);
  } else if (msg->__id__.type == MsgTypeRequest) {
    return putRequest(msg, from_tsp, timeout_ms);
  } else if (msg->__id__.type == MsgTypeResponse) {
    return putResponse(msg, from_tsp, timeout_ms);
  } else if (msg->__id__.type == MsgTypeServiceBroadcast) {
    return putServiceBroadcast(msg, from_tsp, timeout_ms);
  } else if (msg->__id__.type == MsgTypeServiceDiscovery) {
    return putServiceDiscovery(msg, from_tsp, timeout_ms);
  } else {
    UROS_PRINT("Unknow msg type: %d\n", msg_->__id__.type);
    return false;
  }
}

inline bool TransportLocal::putNormal(MsgBase *msg, int from_tsp,
                                      int timeout_ms) {
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

inline bool TransportLocal::putRequest(MsgBase *msg, int from_tsp,
                                       int timeout_ms) {
  return false; // TODO
}

inline bool TransportLocal::putResponse(MsgBase *msg, int from_tsp,
                                        int timeout_ms) {
  return false; // TODO
}

inline bool TransportLocal::putServiceBroadcast(MsgBase *msg, int from_tsp,
                                                int timeout_ms) {
  return false; // TODO
}

inline bool TransportLocal::putServiceDiscovery(MsgBase *msg, int from_tsp,
                                                int timeout_ms) {
  return false; // TODO
}

template <typename Topic>
void TransportLocal::sendMsg(Topic *topic, const typename Topic::Msg &msg) {
  topic->update(msg);
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
