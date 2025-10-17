#pragma once

#include "publisher.h"
#include "topic.h"

namespace uros {

template <typename Msg> void PublisherT<Msg>::advertise(TopicT<Msg> *topic) {
  topic_ = topic;
}

template <typename TMsg> void PublisherT<TMsg>::publish(const TMsg &msg) {
  msg.__meta__.type = MsgTypeNormal;
  msg.__meta__.id.msg.topic = topic_->id();
  msg.__meta__.id.msg.seq = seq_++;
  topic_->write(msg);
}

} // namespace uros
