#pragma once

#include "publisher.h"
#include "system.h"
#include "topic.h"

// ============================================================
// publisher.hpp — PublisherT 实现
// ============================================================

namespace uros {

// advertise(): 将自身绑定到 topic，仅保存指针即可
template <typename Msg> void PublisherT<Msg>::advertise(TopicT<Msg> *topic) {
  topic_ = topic;
}

// publish(): 在发布消息前由中间件自动填写元数据：
//   - type       = MsgTypeNormal（普通话题消息）
//   - sys_src    = 本节点 ID
//   - entry_hash = 所属 topic 的哈希 ID
//   - len        = 消息体有效长度（ByteStream 由调用方通过 setLen 指定，其余类型自动计算）
// 填写完毕后交由 Topic::write() 完成本地分发和跨节点转发
template <typename TMsg> void PublisherT<TMsg>::publish(const TMsg &msg) {
  msg.__meta__.type = MsgTypeNormal;
  msg.__meta__.sys_src = System::Id();
  msg.__meta__.entry_hash = topic_->id();

  if constexpr (!std::is_same_v<TMsg, ByteStream>) {
    msg.__meta__.len = sizeof(TMsg) - sizeof(MsgBase);
  }

  topic_->write(this, msg);
}

} // namespace uros
