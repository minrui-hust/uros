#pragma once

#include "subscription.h"
#include "topic.h"

namespace uros {
template <typename TMsg>
void SubscriptionT<TMsg>::subscribe(
    Topic *topic, const std::function<void(const Msg &)> &cb) {
  cb_ = cb;
  topic_ = topic;
}

template <typename TMsg> void SubscriptionT<TMsg>::spinOnce() {
  TMsg msg;
  if (topic_->read(msg, generation_)) {
    cb_(msg);
  }
}

} // namespace uros
