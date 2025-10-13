#pragma once

#include "topic.h"
#include "transport.h"

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
  // TODO: more field

  UROS_PRINT("add topic '%s' to transport %d succeed\n", topic_name, id_);

  return true;
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

template <typename... TTransports>
TransportManagerT<TTransports...>::TransportManagerT() {
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (([&]() {
       auto &tsp = transport<Is>();
       tsp.setId(Is).setRouter(&router_);
       router_.addTransport(&tsp);
     }()),
     ...);
  }(std::make_index_sequence<size>{});
}

template <typename... TTransports>
void TransportManagerT<TTransports...>::init() {
  std::apply(
      [&](auto &&...transports) {
        (([&](auto &&transport) { transport.init(); }(transports)), ...);
      },
      transports_);
}

} // namespace uros
