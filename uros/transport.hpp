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
  if (topic_id >= topic_metas_.size()) {
    return false;
  }

  topic_metas_[topic_id].name = topic_name;
  topic_metas_[topic_id].topic = topic;

  topic->setTransport(id_, this);

  UROS_PRINT("add topic '%s' to transport %d succeed\n", topic_name, id_);

  return true;
}

template <typename... TTransports>
TransportManagerT<TTransports...>::TransportManagerT() {
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (([&]() { transport<Is>().id() = Is; }()), ...);
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
