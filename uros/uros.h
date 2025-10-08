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

  static void Init() { TransportManager::Init(); }

  template <size_t Idx> static auto &Transport() {
    return TransportManager::template Transport<Idx>();
  }
};

} // namespace uros
