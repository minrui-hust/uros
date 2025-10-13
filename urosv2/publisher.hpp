#pragma once

#include "publisher.h"
#include "topic.h"

namespace uros {

template <typename Msg> void PublisherT<Msg>::advertise(TopicT<Msg> *topic) {
  topic_ = topic;
}

template <typename TMsg> void PublisherT<TMsg>::publish(const TMsg &msg) {
  msg.__id__.participant = id_;
  msg.__id__.type = MsgTypeNormal;
  topic_->write(msg);
}

} // namespace uros
