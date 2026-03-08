#pragma once

#include "platform.h"

// ============================================================
// subscription.h — 订阅者（Subscription）定义
//
// Subscription 是话题（Topic）读取端的句柄。
// 用户通过 Node::createSubscription<Msg>(topic_name, cb) 获取，
// 话题有新消息时，Node::spinOnce() 会调用 spinOnce() 触发回调。
//
// 设计要点：
//   - Subscription 由 Topic 持有，生命周期由 Topic 管理。
//   - 每个 Subscription 占用 Node 的一个事件位（bit），
//     topic 更新时通过 EventGroup::set() 唤醒 Node 的 spin。
//   - version_ 与 Topic 内部的 version_ 比较以判断是否有新数据，
//     避免重复处理同一条消息。
// ============================================================

namespace uros {

template <typename TMsg> struct TopicT;

// SubscriptionBase: 所有订阅者的基类
struct SubscriptionBase {
  SubscriptionBase(int id) : id_(id) {}

  // setupEvent(): 将该订阅绑定到 Node 的事件组，并分配一个位掩码
  // 每个订阅/服务端占一个位，最多支持 24 个（受 EventBits 宽度限制）
  void setupEvent(EventGroup *evt, int bit_idx) {
    CHECK(evt)
    CHECK(bit_idx < 24);
    evt_ = evt;
    bit_mask_ = 1 << bit_idx;
  }

  const auto &bitMask() const { return bit_mask_; }

  // notify(): 由 Topic 在写入新数据后调用，唤醒等待中的 Node::spinOnce()
  void notify() {
    UROS_PRINT("sub(srv) '%d' notified\n", id_);
    evt_->set(bit_mask_);
  }

  // spinOnce(): 检查是否有新版本数据并执行回调，由 Node::spinOnce() 调用
  virtual void spinOnce() = 0;

  virtual ~SubscriptionBase() = default;

protected:
  int id_;              // 在 topic.subs_ 数组中的下标
  EventGroup *evt_ = nullptr;  // 指向所属 Node 的事件组
  EventBits bit_mask_ = 0;     // 该订阅占用的事件位掩码
  int version_ = -1;           // 上次读取时的消息版本号，用于新旧比较
};

// SubscriptionT<TMsg>: 模板化订阅者，绑定到特定消息类型的 TopicT<TMsg>
template <typename TMsg> struct SubscriptionT : public SubscriptionBase {
  using Topic = TopicT<TMsg>;
  using Msg = TMsg;

  SubscriptionT(int id) : SubscriptionBase(id) {}

  // subscribe(): 将自身与 topic 及回调绑定，由 Topic::addSubscription() 调用
  void subscribe(Topic *topic, const std::function<void(const Msg &)> &cb);

  // spinOnce(): 尝试从 topic 读取最新消息（版本更新才读），执行用户回调
  void spinOnce() override;

protected:
  Msg msg_;                            // 本地消息缓冲
  Topic *topic_;                       // 关联的 Topic 指针
  std::function<void(const Msg &)> cb_; // 用户注册的消息回调
};

} // namespace uros
