#pragma once

#include "publisher.h"
#include "topic.h"

namespace uros {

template <typename Msg> void PublisherT<Msg>::advertise(TopicT<Msg> *topic) {
  topic_ = topic;
}

template <typename TMsg> void PublisherT<TMsg>::publish(const TMsg &msg) {
  // TODO: __id__
  topic_->write(msg);
}

} // namespace uros
