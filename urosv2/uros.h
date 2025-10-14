#pragma once

#include "node.hpp"
#include "publisher.hpp"
#include "subscription.hpp"
#include "topic.hpp"
#include "transport.hpp"
#include "transport_local.hpp"
#include "transport_remote.hpp"

namespace uros {

template <typename Msg>
static auto RegisterTopic(const char *topic_name, const uint8_t topic_id,
                          const uint8_t topic_prio = 0) {
  return TopicManager::AddTopic<TopicT<Msg>>(topic_name, topic_id, topic_prio);
}

template <typename Transport> static auto RegisterTransport() {
  return TransportManager::AddTransport<Transport>();
}

static TransportLocal *GetTransportLocal() {
  return TransportManager::GetTransportLocal();
}

static void Init() {
  TransportManager::Init();
  // TODO: maybe init others
}

} // namespace uros
