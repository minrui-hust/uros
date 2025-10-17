#pragma once

#include "platform.h"

#include "topic.h"

#include "publisher.h"
#include "subscription.h"

namespace uros {

inline bool TopicBase::addTransport(TransportBase *tsp) {
  if (tsps_.full()) {
    return false;
  }

  tsps_.emplace_back(tsp);

  return true;
}

template <typename Msg> PublisherT<Msg> *TopicT<Msg>::addPublisher() {
  if (pubs_.full()) {
    return nullptr;
  }

  auto pub = new PublisherT<Msg>(pubs_.size());
  CHECK(pub);

  pub->advertise(this);
  pubs_.emplace_back(pub);

  return pub;
}

template <typename Msg>
SubscriptionT<Msg> *
TopicT<Msg>::addSubscription(const std::function<void(const Msg &)> &cb) {
  if (subs_.full()) {
    return nullptr;
  }

  auto sub = new SubscriptionT<Msg>(subs_.size());
  CHECK(sub);

  sub->subscribe(this, cb);
  subs_.emplace_back(sub);

  return sub;
}

template <typename TMsg>
int16_t TopicT<TMsg>::write(const TMsg &msg, TransportBase *from_tsp) {
  { // update msg in critical section
    LockGuard<CriticalLock> lg;
    if (msg.__meta__.id.msg.seq - msg_.__meta__.id.msg.seq <= 0) {
      return msg_.__meta__.id.msg.seq;
    }
    msg_ = msg;
  }

  // first notify subs, cause higher prior work may waken
  for (auto &sub : subs_) {
    sub->notify();
  }

  // then notify tsps
  for (auto &tsp : tsps_) {
    if (tsp != from_tsp) {
      tsp->notify(this);
    }
  }

  return msg.__meta__.id.msg.seq;
}

template <typename TMsg>
int16_t TopicT<TMsg>::write(const MsgBase *msg, TransportBase *from_tsp) {
  return write(*static_cast<const TMsg *>(msg), from_tsp);
}

template <typename TMsg> bool TopicT<TMsg>::read(TMsg &msg, int16_t &seq) {
  LockGuard<CriticalLock> lg;
  if (msg_.__meta__.id.msg.seq - seq <= 0) {
    return false;
  }
  msg = msg_;
  seq = msg_.__meta__.id.msg.seq;
  return true;
}

template <typename TMsg> bool TopicT<TMsg>::read(MsgBase *msg, int16_t &seq) {
  return read(*static_cast<TMsg *>(msg), seq);
}

template <typename Topic>
Topic *TopicManager::addTopic(const char *name, uint8_t id, uint8_t prio) {
  if ((size_t)id >= topics_.size()) {
    return nullptr;
  }

  auto &topic = topics_[id];
  if (topic) {
    if (topic->id() == id && strcmp(topic->name(), name) == 0) {
      UROS_PRINT("topic '%s' already added with same type\n", name);
      return static_cast<Topic *>(topic.get());
    } else {
      UROS_PRINT("topic '%s' already added with different type\n", name);
      return nullptr;
    }
  }

  topic = etl::unique_ptr(new Topic(name, id, prio));
  CHECK(topic);

  return static_cast<Topic *>(topic.get());
}

template <typename Topic> Topic *TopicManager::findTopic(const char *name) {
  for (auto i = 0u; i < topics_.size(); ++i) {
    auto &tp = topics_[i];
    if (tp.get() != nullptr && strcmp(tp->name(), name) == 0) {
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
