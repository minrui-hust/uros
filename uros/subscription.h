#pragma once

#include "platform.h"

namespace uros {

template <typename TransportManager, typename TMsg> struct TopicT;

struct SubscriptionBase {
  SubscriptionBase(EventGroup *evt, int32_t idx)
      : evt_(evt), bit_mask_(1 << idx) {
    CHECK(evt);
    CHECK(idx < 24); // only 24 bits for event
  }

  const EventBits &bitMask() const { return bit_mask_; }

  void notify() { evt_->set(bit_mask_); }

  virtual void spinOnce() = 0;

protected:
  EventGroup *evt_;
  EventBits bit_mask_;
  int32_t generation_ = -1;
};

template <typename TransportManager, typename TMsg>
struct SubscriptionT : public SubscriptionBase {
  using Topic = TopicT<TransportManager, TMsg>;

  SubscriptionT(EventGroup *evt, int32_t idx) : SubscriptionBase(evt, idx) {}

  void subscribe(Topic *topic, const std::function<void(const TMsg &)> &cb);

  void spinOnce() override;

protected:
  Topic *topic_;
  std::function<void(const TMsg &)> cb_;
};

} // namespace uros
