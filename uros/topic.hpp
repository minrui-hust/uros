#pragma once

#include "platform.h"

#include "topic.h"

#include "publisher.h"
#include "subscription.h"
#include "transport.h"

namespace uros {

inline bool TopicBase::registerTransport(TransportBase *tsp) {
  auto tsp_id = tsp->id();
  if (tsp_id > tsps_.size()) {
    return false;
  }

  if (tsps_[tsp_id]) {
    UROS_PRINT("tsp '%d' already register on topic %d \n", tsp_id, id_);
    return false;
  }

  tsps_[tsp_id] = tsp;

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
void TopicT<TMsg>::write(Publisher *pub, const TMsg &msg) {
  if (update(msg, msg.__meta__.id.msg.seq)) {
    notify(nullptr);
  }
}

template <typename TMsg>
void TopicT<TMsg>::write(TransportBase *tsp, const MsgBase *msg) {
  if (update(*static_cast<const TMsg *>(msg), msg->__meta__.id.msg.seq)) {
    notify(tsp);
  }
}

template <typename TMsg> bool TopicT<TMsg>::read(TMsg &msg, int &seq) {
  LockGuard<CriticalLock> lg;
  if (int16_t(msg_.__meta__.id.msg.seq - seq) > 0) {
    msg = msg_;
    seq = msg_.__meta__.id.msg.seq;
    return true;
  }
  return false;
}

template <typename TMsg> bool TopicT<TMsg>::read(MsgBase *msg, int &seq) {
  return read(*static_cast<TMsg *>(msg), seq);
}

template <typename TMsg> bool TopicT<TMsg>::update(const TMsg &msg, int seq) {
  LockGuard<CriticalLock> lg;
  if (int16_t(seq - msg_.__meta__.id.msg.seq) > 0) {
    msg_ = msg;
    msg_.__meta__.id.msg.seq = seq;
    return true;
  }
  return false;
}

template <typename TMsg> void TopicT<TMsg>::notify(TransportBase *from_tsp) {
  // first notify subs, cause higher prior work may waken
  for (auto &sub : subs_) {
    sub->notify();
  }

  // then notify tsps
  for (auto &tsp : tsps_) {
    if (tsp && tsp != from_tsp) {
      tsp->notify(this);
    }
  }
}

template <typename Topic>
Topic *TopicManager::addTopic(const char *name, uint8_t id, uint8_t prio) {
  if (topics_.full()) {
    return nullptr;
  }

  // create a new topic
  auto topic = new Topic(name, id, prio);
  CHECK(topic);
  topics_.emplace_back(topic);

  return topic;
}

template <typename Topic> Topic *TopicManager::findTopic(const char *name) {
  for (auto &tp : topics_) {
    if (tp.get() != nullptr && strcmp(tp->name(), name) == 0) {
      if constexpr (std::is_same_v<Topic, TopicBase>) {
        return tp.get();
      } else {
        if (tp->msgType() == type_id<typename Topic::Msg>()) {
          return static_cast<Topic *>(tp.get());
        } else {
          UROS_PRINT("topic found but msg type mismatch\n");
          return nullptr;
        }
      }
    }
  }

  return nullptr;
}

} // namespace uros
