#pragma once

#include "platform.h"

#include "topic.h"

#include "publisher.h"
#include "subscription.h"
#include "transport.h"

// ============================================================
// topic.hpp — TopicBase / TopicT / TopicManager 实现
// ============================================================

namespace uros {

// registerTransport(): 将传输层 tsp 注册到本话题的 tsps_ 数组。
// tsps_ 以 Transport ID 为下标（稀疏数组），最多 UROS_MAX_TRANSPORTS 个。
// 注册后，当话题有新消息时，该传输层的发送线程会被唤醒以转发消息。
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

// addPublisher(): 在话题上创建并注册一个发布者。
// Publisher 由 Topic 持有（unique_ptr），Node 只保存裸指针。
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

// addSubscription(): 在话题上创建并注册一个订阅者，绑定用户回调。
// Subscription 由 Topic 持有（unique_ptr），Node 只保存裸指针。
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

// write(pub, msg): 本地发布者写入消息。
// from_tsp 传 nullptr，notify() 会通知所有已注册的传输层。
template <typename TMsg>
void TopicT<TMsg>::write(Publisher *pub, const TMsg &msg) {
  update(msg);
  notify(nullptr);
}

// write(tsp, msg): 传输层写入远端消息。
// from_tsp 非 nullptr，notify() 会跳过该传输层（防止消息回路）。
template <typename TMsg>
void TopicT<TMsg>::write(TransportBase *tsp, const MsgBase *msg) {
  update(*static_cast<const TMsg *>(msg));
  notify(tsp);
}

// read(msg, ver): 读取最新消息（临界区保护）。
// 使用版本号而非信号量，保证订阅者每次 spinOnce() 均能获取到最新值，
// 并避免消息积压。返回值为消息字节数（含头部），0 表示无新数据。
template <typename TMsg> size_t TopicT<TMsg>::read(TMsg &msg, int &ver) {
  LockGuard<CriticalLock> lg;
  if (int(version_ - ver) > 0) {
    msg = msg_;
    ver = version_;
    return msg.__meta__.len + sizeof(MsgBase);
  }
  return 0;
}

template <typename TMsg> size_t TopicT<TMsg>::read(MsgBase *msg, int &seq) {
  return read(*static_cast<TMsg *>(msg), seq);
}

// update(): 在临界区内原子地更新消息缓冲并递增版本号。
// 版本号用有符号差值比较（int(ver - local_ver) > 0），
// 可正确处理 32 位整数回绕。
template <typename TMsg> bool TopicT<TMsg>::update(const TMsg &msg) {
  LockGuard<CriticalLock> lg;
  msg_ = msg;
  ++version_;
  return true;
}

// notify(): 按顺序通知本地订阅者和传输层。
// 先通知订阅者（可能唤醒高优先级任务），再通知传输层（触发发送线程）。
// from_tsp 为消息来源传输层，发送时跳过该传输层以避免回路。
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

// addTopic(): 创建新话题并加入全局列表。
// topics_ 使用 unique_ptr 持有，自动管理生命周期。
template <typename Topic>
Topic *TopicManager::addTopic(const char *name, int prio) {
  if (topics_.full()) {
    return nullptr;
  }

  // create a new topic
  auto topic = new Topic(name, prio);
  CHECK(topic);

  topics_.emplace_back(topic);

  return topic;
}

// findTopic(): 按名称线性查找话题，并验证消息类型。
// 若 Topic 类型为 TopicBase（基类），则跳过类型检查直接返回（用于传输层注册）。
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
