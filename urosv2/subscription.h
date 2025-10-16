#pragma once

#include "platform.h"

namespace uros {

template <typename TMsg> struct TopicT;

struct SubscriptionBase {
  SubscriptionBase(int id) : id_(id) {}

  void setupEvent(EventGroup *evt, int bit_idx) {
    CHECK(evt)
    CHECK(bit_idx < 24);
    evt_ = evt;
    bit_mask_ = 1 << bit_idx;
  }

  const auto &bitMask() const { return bit_mask_; }

  void notify() { evt_->set(bit_mask_); }

  virtual void spinOnce() = 0;

  virtual ~SubscriptionBase() = default;

protected:
  int id_; // index in topic
  EventGroup *evt_ = nullptr;
  EventBits bit_mask_ = 0;
  int generation_ = -1;
};

template <typename TMsg> struct SubscriptionT : public SubscriptionBase {
  using Topic = TopicT<TMsg>;
  using Msg = TMsg;

  SubscriptionT(int id) : SubscriptionBase(id) {}

  void subscribe(Topic *topic, const std::function<void(const Msg &)> &cb);

  void spinOnce() override;

protected:
  Topic *topic_;
  std::function<void(const Msg &)> cb_;
};

} // namespace uros
