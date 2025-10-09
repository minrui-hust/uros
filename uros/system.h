#pragma once

#include "node.hpp"
#include "publisher.hpp"
#include "subscription.hpp"
#include "topic.hpp"
#include "transport.hpp"

namespace uros {

template <typename... Transports> struct System {
  using TransportManager = TransportManagerT<Transports...>;
  using Node = NodeT<TransportManager>;

  template <typename TMsg>
  using PublisherHandleT = PublisherT<TransportManager, TMsg> *;

  template <typename TMsg>
  using SubscriptionHandleT = SubscriptionT<TransportManager, TMsg> *;

  using Message = MsgBase;

  template <typename TMsg>
  static bool RegisterTopic(const char *topic_name, const int32_t topic_id) {
    auto topic = TopicManager::AddTopic<TopicT<TransportManager, TMsg>>(
        topic_name, topic_id);
    return topic != nullptr;
  }

  template <size_t Idx> static auto &Transport() {
    return TransportManager::template Transport<Idx>();
  }

  template <typename TTransport> static auto &Transport() {
    return TransportManager::template Transport<TTransport>();
  }

  static void Init() {
    TransportManager::Init();
    // TODO: maybe init others
  }
};

} // namespace uros
