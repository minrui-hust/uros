#pragma once

#include "node.hpp"
#include "publisher.hpp"
#include "subscription.hpp"
#include "topic.hpp"
#include "transport.hpp"
#include "transport_local.hpp"

namespace uros {

template <typename Msg>
static auto RegisterTopic(const char *topic_name, const uint8_t topic_id,
                          const uint8_t topic_prio = 0) {
  return TopicManager::AddTopic<TopicT<Msg>>(topic_name, topic_id);
}

template <size_t Idx> static auto &Transport() {
  return TRANSPORTS_MANAGER::template Transport<Idx>();
}

template <typename T> static auto &Transport() {
  return TRANSPORTS_MANAGER::template Transport<T>();
}

static void Init() {
  TRANSPORTS_MANAGER::Init();
  // TODO: maybe init others
}

} // namespace uros
