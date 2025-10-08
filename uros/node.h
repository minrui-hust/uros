#pragma once

#include "etl/vector.h"

#include "platform.h"

namespace uros {

struct SubscriptionBase;
template <typename TransportManager, typename TMsg> struct SubscriptionT;

struct PublisherBase;
template <typename TransportManager, typename TMsg> struct PublisherT;

struct TopicBase;
template <typename TransportManager, typename TMsg> struct TopicT;

template <typename TransportManager> struct NodeT {

  template <typename TMsg>
  SubscriptionT<TransportManager, TMsg> *
  createSubscription(const char *topic_name,
                     const std::function<void(const TMsg &)> &cb);

  template <typename TMsg>
  PublisherT<TransportManager, TMsg> *createPublisher(const char *topic_name);

  void spin();
  void spinOnce(int32_t timeout_ms = -1);

protected:
  EventGroup evt_;
  EventBits wait_set_ = 0;

  etl::vector<std::unique_ptr<SubscriptionBase>, UROS_NODE_MAX_SUBS> subs_;
  etl::vector<std::unique_ptr<PublisherBase>, UROS_NODE_MAX_PUBS> pubs_;
};

} // namespace uros
