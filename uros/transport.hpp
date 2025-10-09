#pragma once

#include "topic.h"
#include "transport.h"

namespace uros {

template <typename Derived>
bool TransportBase<Derived>::declareTopic(const char *topic_name) {
  auto topic = TopicManager::FindTopic<TopicBase>(topic_name);
  if (!topic) {
    return false;
  }

  auto topic_id = topic->id();
  if (topic_id >= topics_.size()) {
    UROS_PRINT("invalide topic id: %d\n", topic_id);
    return false;
  }

  topics_[topic_id] = topic;
  topic->setTransport(id_, this);

  UROS_PRINT("add topic '%s' to transport %d succeed\n", topic->name(), id_);

  return true;
}

template <typename... TTransports>
TransportManagerT<TTransports...>::TransportManagerT() {
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (([&]() { transport<Is>().id() = Is; }()), ...);
  }
  (std::make_index_sequence<size>{});
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
