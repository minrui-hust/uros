#pragma once

#include "topic.h"
#include "transport.h"

namespace uros {

template <typename Derived>
bool TransportBase<Derived>::declareTopic(const char *topic_name,
                                          const int32_t topic_id) {
  if (topic_id >= topic_metas_.size()) {
    return false;
  }

  topic_metas_[topic_id].name = topic_name;

  return true;
}

template <typename Derived>
int32_t TransportBase<Derived>::registerTopic(TopicBase *topic) {
  // only previously declared topic can be registered
  for (auto i = 0u; i < topic_metas_.size(); ++i) {
    auto &meta = topic_metas_[i];
    UROS_PRINT("meta.name: '%s', topic.name: '%s'\n", meta.name, topic->name());
    if (etl::strcmp(meta.name, topic->name()) == 0) {
      meta.topic = topic;
      return i;
    }
  }
  return -1;
}

template <typename... TTransports>
TransportManagerT<TTransports...>::TransportManagerT() {
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (([&]() { transport<Is>().id() = Is; }()), ...);
  }(std::make_index_sequence<size>{});
}

template <typename... TTransports>
template <typename Topic>
int32_t TransportManagerT<TTransports...>::registerTopic(Topic *topic) {
  int32_t topic_id = -1;
  bool id_consistent = true;

  // iterate on all transport
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (([&]() {
       auto &tsp = transport<Is>();
       auto tmp_id = tsp.registerTopic(topic);
       if (tmp_id >= 0) {
         if (topic_id == -1 || topic_id == tmp_id) {
           topic_id = tmp_id;
           topic->template setTransport<Is>(&tsp);
           UROS_PRINT("register topic '%s' on transport %d succeed\n",
                      topic->name(), int(Is));
         } else {
           UROS_PRINT("!!! topic id inconsistent !!!\n");
           id_consistent = false;
         }
       } else {
         UROS_PRINT("failed to register topic '%s' on transport %d\n",
                    topic->name(), int(Is));
       }
     }()),
     ...);
  }(std::make_index_sequence<size>{});

  return id_consistent ? topic_id : -1;
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
