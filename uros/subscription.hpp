#pragma once

#include "subscription.h"
#include "topic.h"

// ============================================================
// subscription.hpp — SubscriptionT 实现
// ============================================================

namespace uros {

// subscribe(): 保存 topic 指针和回调，不立即读取数据
template <typename TMsg>
void SubscriptionT<TMsg>::subscribe(
    Topic *topic, const std::function<void(const TMsg &)> &cb) {
  cb_ = cb;
  topic_ = topic;
}

// spinOnce(): 由 Node::spinOnce() 在对应事件位触发时调用。
// topic_->read() 以版本比较的方式读取数据：
//   - 若 topic 版本 > 本地 version_，则读取最新数据并更新 version_，返回长度 > 0
//   - 否则返回 0，表示自上次处理后无新消息
// 读到新消息后调用用户注册的回调 cb_()
template <typename TMsg> void SubscriptionT<TMsg>::spinOnce() {
  if (topic_->read(msg_, version_)) {
    cb_(msg_);
  }
}

} // namespace uros
