#pragma once

#include "subscription.h"
#include "topic.h"

namespace uros {
template <typename TMsg>
void SubscriptionT<TMsg>::subscribe(
    Topic *topic, const std::function<void(const TMsg &)> &cb) {
  cb_ = cb;
  topic_ = topic;
}

template <typename TMsg> void SubscriptionT<TMsg>::spinOnce() {
  if (topic_->read(msg_, version_)) {
    cb_(msg_);
  }
}

} // namespace uros
