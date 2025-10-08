#pragma once

#include "platform.h"

#include "topic.h"

#include "publisher.h"
#include "subscription.h"
#include "transport_local.h"

namespace uros {

template <typename TransportManager, typename TMsg>
template <size_t Idx, typename Transport>
void TopicT<TransportManager, TMsg>::setTransport(Transport *transport) {
  std::get<Idx>(transports_) = transport;
}

template <typename TransportManager, typename TMsg>
void TopicT<TransportManager, TMsg>::write(const TMsg &msg) {
  UROS_PRINT("Write message to topic '%s'\n", name());

  // first write to TransportLocal
  auto &transport_local = std::get<TransportLocal *>(transports_);
  if (transport_local) {
    transport_local->write(this, msg);
  }

  // broadcast write on all other transport
  std::apply(
      [&](auto &&...transports) {
        (([&](auto &&transport) {
           // skip TransportLocal, processed already
           if constexpr (!std::is_same_v<std::decay_t<decltype(transport)>,
                                         TransportLocal *>) {
             if (transport != nullptr) {
               transport->write(this, msg);
             }
           }
         }(transports)),
         ...);
      },
      transports_);
}

template <typename TransportManager, typename TMsg>
bool TopicT<TransportManager, TMsg>::read(TMsg &msg, int32_t &gen) {
  LockGuard<CriticalLock> guard;
  if (generation_ <= gen) {
    return false;
  }
  msg = msg_;
  gen = generation_;
  return true;
}

template <typename TransportManager, typename TMsg>
void TopicT<TransportManager, TMsg>::update(const TMsg &msg) {
  { // copy msg in critical section
    LockGuard<CriticalLock> guard;
    msg_ = msg;
    ++generation_;
  }

  // notify subscriber to consume it
  // this should be done before route msg,
  // cause higher priority task may be waken
  for (auto i = 0u; i < subs_.size(); ++i) {
    subs_[i]->notify();
  }
}

template <typename TransportManager, typename TMsg>
void TopicT<TransportManager, TMsg>::recv(const int32_t tsp_id,
                                          const MsgBase *msg) {
  // first route to TransportLocal
  auto &transport_local = std::get<TransportLocal *>(transports_);
  if (transport_local) {
    transport_local->write(this, *static_cast<const TMsg *>(msg));
  }

  // route msg to other non-local transport with id other than tsp_id
  std::apply(
      [&](auto &&...transports) {
        (([&](auto &&transport) {
           // skip TransportLocal
           if constexpr (!std::is_same_v<std::decay_t<decltype(transport)>,
                                         TransportLocal *>) {
             if (transport != nullptr && transport->id() != tsp_id) {
               transport->write(this, *static_cast<const TMsg *>(msg));
             }
           }
         }(transports)),
         ...);
      },
      transports_);
}

template <typename Topic> Topic *TopicManager::findOrAdd(const char *name) {
  using TransportManager = typename Topic::TransportManager;
  using Msg = typename Topic::Msg;

  // find first
  for (auto i = 0u; i < topics_.size(); ++i) {
    auto &tp = topics_[i];
    if (strcmp(tp->name(), name) == 0) {
      if (tp->msgType() == type_id<Msg>()) {
        return static_cast<Topic *>(tp.get());
      } else {
        UROS_PRINT("topic found but msg type mismatch\n");
        return nullptr; // topic exist but type mismatch
      }
    }
  }

  // not found, try add
  if (topics_.full()) {
    UROS_PRINT("Max topic number reached\n");
    return nullptr;
  }

  auto tp = std::make_unique<Topic>(name);
  UROS_ASSERT(tp);

  tp->id() = TransportManager::RegisterTopic(tp.get());
  if (tp->id() < 0) {
    UROS_PRINT("Failed to register topic on any transport\n");
    return nullptr;
  }
  UROS_PRINT("topic_id for '%s': %d\n", tp->name(), tp->id());

  return static_cast<Topic *>(topics_.emplace_back(std::move(tp)).get());
}

} // namespace uros
