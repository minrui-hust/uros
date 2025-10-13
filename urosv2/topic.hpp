#pragma once

#include "platform.h"

#include "topic.h"

#include "publisher.h"
#include "subscription.h"
#include "transport_local.h"

namespace uros {

template <typename Msg>
SubscriptionT<Msg> *
TopicT<Msg>::addSubscription(const std::function<void(const Msg &)> &cb) {
  auto sub = etl::unique_ptr(new SubscriptionT<Msg>(subs_.size()));
  CHECK(sub);

  sub->subscribe(this, cb);

  return static_cast<SubscriptionT<Msg> *>(
      subs_.emplace_back(etl::move(sub)).get());
}

template <typename Msg> PublisherT<Msg> *TopicT<Msg>::addPublisher() {
  auto pub = etl::unique_ptr(new PublisherT<Msg>(pubs_.size()));
  CHECK(pub);

  pub->advertise(this);

  return static_cast<PublisherT<Msg> *>(
      pubs_.emplace_back(etl::move(pub)).get());
}

template <typename TMsg> void TopicT<TMsg>::write(const TMsg &msg) {
  TRANSPORTS_MANAGER::Transport<TransportLocal>().sendMsg(this, msg);
}

template <typename TMsg> bool TopicT<TMsg>::read(TMsg &msg, int32_t &gen) {
  LockGuard<CriticalLock> guard;
  if (generation_ <= gen) {
    return false;
  }
  msg = msg_;
  gen = generation_;
  return true;
}

template <typename TMsg> void TopicT<TMsg>::update(const TMsg &msg) {
  { // update msg in critical section
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

template <typename TMsg> void TopicT<TMsg>::recv(const MsgBase *msg) {
  update(*static_cast<TMsg *>(msg));
}

template <typename Topic>
Topic *TopicManager::addTopic(const char *name, uint8_t id, uint8_t prio) {
  if ((size_t)id >= topics_.size()) {
    return nullptr;
  }

  // TODO prio

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
  for (auto i = 0u; i < topics_.size(); ++i) {
    auto &tp = topics_[i];
    if (tp != nullptr && strcmp(tp->name(), name) == 0) {
      if constexpr (std::is_same_v<Topic, TopicBase>) {
        return tp.get();
      } else {
        if (tp->msgType() == type_id<typename Topic::Msg>()) {
          return static_cast<Topic *>(tp.get());
        } else {
          UROS_PRINT("topic found but msg type mismatch\n");
          return nullptr; // topic exist but type mismatch
        }
      }
    }
  }

  return nullptr;
}

} // namespace uros
