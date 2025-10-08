#pragma once

#include "publisher.h"

namespace uros {

template <typename TransportManager, typename TMsg>
void PublisherT<TransportManager, TMsg>::advertise(
    TopicT<TransportManager, TMsg> *topic) {
  topic_ = topic;
}

template <typename TransportManager, typename TMsg>
void PublisherT<TransportManager, TMsg>::publish(const TMsg &msg) {
  topic_->write(msg);
}

} // namespace uros
