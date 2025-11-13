#pragma once

#include "publisher.h"
#include "system.h"
#include "topic.h"

namespace uros {

template <typename Msg> void PublisherT<Msg>::advertise(TopicT<Msg> *topic) {
  topic_ = topic;
}

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
