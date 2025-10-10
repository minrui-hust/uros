#pragma once

#include "platform.h"

#include "topic.h"

#include "publisher.h"
#include "subscription.h"

namespace uros {

template <typename TransportManager, typename TMsg>
void TopicT<TransportManager, TMsg>::write(const TMsg &msg) {
  // iterate on all transport
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (([&]() {
       using TransportType =
           typename TransportManager::template TransportType<Is>;
       auto tsp = static_cast<TransportType *>(transports_[Is]);
       if (tsp != nullptr) {
         tsp->write(this, msg);
       }
     }()),
     ...);
  }
  (std::make_index_sequence<TransportManager::size>{});
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

  // iterate on all transport
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (([&]() {
       using TransportType =
           typename TransportManager::template TransportType<Is>;
       auto tsp = static_cast<TransportType *>(transports_[Is]);
       if (tsp != nullptr && tsp->id() != tsp_id) {
         tsp->write(this, *static_cast<const TMsg *>(msg));
       }
     }()),
     ...);
  }
  (std::make_index_sequence<TransportManager::size>{});
}

template <typename TransportManager, typename TMsg>
void TopicT<TransportManager, TMsg>::setTransport(const int32_t tsp_id,
                                                  TransportInterface *tsp) {
  if ((size_t)tsp_id < transports_.size()) {
    transports_[tsp_id] = tsp;
  }
}

template <typename Topic>
Topic *TopicManager::addTopic(const char *name, int32_t id) {
  if ((size_t)id >= topics_.size()) {
    return nullptr;
  }

  auto &topic = topics_[id];
  if (topic) {
    if (topic->id() == id && strcmp(topic->name(), name) == 0) {
      return static_cast<Topic *>(topic.get());
    } else {
      return nullptr;
    }
  }

  topic = std::make_unique<Topic>(name, id);
  CHECK(topic);

  return static_cast<Topic *>(topic.get());
}

template <typename Topic> Topic *TopicManager::findTopic(const char *name) {
  // find first
  for (auto i = 0u; i < topics_.size(); ++i) {
    auto &tp = topics_[i];
    if (tp != nullptr && strcmp(tp->name(), name) == 0) {
      if constexpr (std::is_same_v<Topic, TopicBase>) {
        return tp.get();
      } else {
        if (tp->msgType() == type_id<typename Topic::Msg>()) {
          return static_cast<Topic *>(tp.get());
        } else {
          // UROS_PRINT("topic found but msg type mismatch\n");
          return nullptr; // topic exist but type mismatch
        }
      }
    }
  }

  return nullptr;
}

} // namespace uros
