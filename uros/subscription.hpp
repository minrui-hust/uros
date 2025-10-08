#pragma once

#include "subscription.h"
#include "topic.h"

namespace uros {

template <typename TransportManager, typename TMsg>
void SubscriptionT<TransportManager, TMsg>::subscribe(
    Topic *topic, const std::function<void(const TMsg &)> &cb) {
  cb_ = cb;
  topic_ = topic;
}

template <typename TransportManager, typename TMsg>
void SubscriptionT<TransportManager, TMsg>::spinOnce() {
  TMsg msg;
  if (topic_->read(msg, generation_)) {
    cb_(msg);
  }
}

} // namespace uros
