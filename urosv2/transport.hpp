#pragma once

#include "topic.h"
#include "transport.h"
#include "transport_local.h"

namespace uros {

inline bool TransportBase::declareTopic(const char *topic_name) {
  auto topic = TopicManager::FindTopic<TopicBase>(topic_name);
  if (!topic) {
    return false;
  }

  auto topic_id = topic->id();
  if ((size_t)topic_id >= topic_metas_.size()) {
    return false;
  }

  auto &topic_meta = topic_metas_[topic_id];
  topic_meta.topic = topic;
  topic_meta.msgs[0] = topic->createMsg();
  topic_meta.msgs[1] = topic->createMsg();

  topic_bit_mask_ |= 1 << topic_id;

  UROS_PRINT("add topic '%s' to transport %d succeed\n", topic_name, id_);

  return true;
}

inline bool TransportBase::put(const MsgBase *msg, int from_tsp,
                               int timeout_ms) {
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
    UROS_PRINT("Unknow msg type: %d\n", msg->__id__.type);
    return false;
  }
}

inline void Router::addTransport(TransportBase *tsp) {
  transports_.emplace_back(tsp);
}

inline bool Router::route(const MsgBase *msg, int from_tsp, int to_tsp,
                          int timeout_ms) {
  if (msg->__id__.type == MsgTypeNormal) {
    return routeNormal(msg, from_tsp, to_tsp, timeout_ms);
  } else if (msg->__id__.type == MsgTypeRequest) {
    return routeRequest(msg, from_tsp, to_tsp, timeout_ms);
  } else if (msg->__id__.type == MsgTypeResponse) {
    return routeResponse(msg, from_tsp, to_tsp, timeout_ms);
  } else if (msg->__id__.type == MsgTypeServiceBroadcast) {
    return routeServiceBroadcast(msg, from_tsp, to_tsp, timeout_ms);
  } else if (msg->__id__.type == MsgTypeServiceDiscovery) {
    return routeServiceDiscovery(msg, from_tsp, to_tsp, timeout_ms);
  } else {
    UROS_PRINT("Unknow msg type: %d\n", msg->__id__.type);
    return false;
  }
}

inline bool Router::routeNormal(const MsgBase *msg, int from_tsp, int to_tsp,
                                int timeout_ms) {
  UROS_PRINT("Route msg: %d, %d\n", msg->__id__.entry, msg->__id__.seq);
  if (to_tsp >= 0 && to_tsp < transports_.size()) {
    return transports_[to_tsp]->put(msg, from_tsp, timeout_ms);
  } else if (to_tsp < 0) {
    return broadcast(msg, from_tsp, timeout_ms);
  } else {
    UROS_PRINT("Invalid to_tsp:%d\n", to_tsp);
    return false;
  }
}

inline bool Router::routeRequest(const MsgBase *msg, int from_tsp, int to_tsp,
                                 int timeout_ms) {
  return false; // TODO
}

inline bool Router::routeResponse(const MsgBase *msg, int from_tsp, int to_tsp,
                                  int timeout_ms) {
  return false; // TODO
}

inline bool Router::routeServiceBroadcast(const MsgBase *msg, int from_tsp,
                                          int to_tsp, int timeout_ms) {
  return false; // TODO
}

inline bool Router::routeServiceDiscovery(const MsgBase *msg, int from_tsp,
                                          int to_tsp, int timeout_ms) {
  return false; // TODO
}

inline bool Router::broadcast(const MsgBase *msg, int from_tsp,
                              int timeout_ms) {
  for (auto &tsp : transports_) {
    if (tsp->id() != from_tsp) {
      tsp->put(msg, from_tsp, timeout_ms);
    }
  }
  return true;
}

inline TransportManager::TransportManager() { addTransport<TransportLocal>(); }

template <typename Transport> Transport *TransportManager::addTransport() {
  if (transports_.full()) {
    return nullptr;
  }

  auto tsp = new Transport;
  assert(tsp);

  tsp->setId(transports_.size()).setRouter(&router_);
  router_.addTransport(tsp);

  transports_.emplace_back(tsp);

  return tsp;
}

inline TransportLocal *TransportManager::getTransportLocal() {
  return static_cast<TransportLocal *>(transports_[0].get());
}

inline void TransportManager::init() {
  for (auto &tsp : transports_) {
    tsp->init();
  }
}

} // namespace uros
