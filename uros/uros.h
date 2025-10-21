#pragma once

#include "node.hpp"
#include "publisher.hpp"
#include "subscription.hpp"
#include "topic.hpp"
#include "transport.hpp"

namespace uros {

template <typename Msg>
static auto RegisterTopic(const char *topic_name, const uint8_t topic_id,
                          const uint8_t topic_prio = 0) {
  static_assert(sizeof(Msg) <= UROS_MSG_MAX_SIZE);
  return TopicManager::AddTopic<TopicT<Msg>>(topic_name, topic_id, topic_prio);
}

template <typename Transport> static auto RegisterTransport() {
  return TransportManager::AddTransport<Transport>();
}

static void Init() {
  TransportManager::Init();
  // TODO: maybe init others
}

} // namespace uros
