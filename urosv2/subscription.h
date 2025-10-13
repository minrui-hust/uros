#pragma once

#include "platform.h"

namespace uros {

template <typename TMsg> struct TopicT;

struct SubscriptionBase {
  SubscriptionBase(int id) : id_(id) {}

  EventBits &bitMask() { return bit_mask_; }
  const EventBits &bitMask() const { return bit_mask_; }

  void notify() {
    // TODO
  }

  virtual void spinOnce() = 0;

protected:
  int id_; // TODO
  EventBits bit_mask_;
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
