#pragma once

#include "node.h"

#include "publisher.h"
#include "service.h"
#include "subscription.h"
#include "topic.h"

namespace uros {

template <typename Msg>
SubscriptionT<Msg> *
Node::createSubscription(const char *topic_name,
                         const std::function<void(const Msg &)> &cb) {
  using Topic = TopicT<Msg>;
  using Subscription = SubscriptionT<Msg>;

  if (subs_.full()) {
    return nullptr;
  }

  auto topic = TopicManager::FindTopic<Topic>(topic_name);
  if (!topic) {
    UROS_PRINT("Failed to find topic: '%s'\n", topic_name);
    return nullptr;
  }

  // add subscription via topic, cause subscription is owned by topic
  auto sub = topic->addSubscription(cb);
  CHECK(sub);

  // set notification bit mask
  sub->setupEvent(&evt_, subs_.size());
  wait_set_ |= sub->bitMask();

  return static_cast<Subscription *>(subs_.emplace_back(sub));
}

template <typename Msg>
PublisherT<Msg> *Node::createPublisher(const char *topic_name, int prio) {
  using Topic = TopicT<Msg>;
  using Publisher = PublisherT<Msg>;

  if (pubs_.full()) {
    return nullptr;
  }

  auto topic = TopicManager::FindTopic<Topic>(topic_name);
  if (!topic) {
    UROS_PRINT("Failed to find topic: '%s'\n", topic_name);
    return nullptr;
  }

  // add publisher via topic, cause publisher is owned by topic
  auto pub = topic->addPublisher();
  CHECK(pub);

  return static_cast<Publisher *>(pubs_.emplace_back(pub));
}

template <typename TReq, typename TRsp>
ClientT<TReq, TRsp> *Node::createClient(const char *service_name) {
  using Service = ServiceT<TReq, TRsp>;
  using Client = ClientT<TReq, TRsp>;

  if (clis_.full()) {
    return nullptr;
  }

  auto service = ServiceManager::FindService<Service>(service_name);
  if (!service) {
    UROS_PRINT("Failed to find service: '%s'\n", service_name);
    return nullptr;
  }

  // add client via service, cause client is owned by service
  auto cli = service->addClient();
  CHECK(cli);

  return static_cast<Client *>(clis_.emplace_back(cli));
}

template <typename TReq, typename TRsp>
ServerT<TReq, TRsp> *
Node::createServer(const char *service_name,
                   const std::function<void(const TReq &, TRsp &)> &cb) {
  using Service = ServiceT<TReq, TRsp>;
  using Server = ServerT<TReq, TRsp>;

  if (subs_.full()) {
    return nullptr;
  }

  auto service = ServiceManager::FindService<Service>(service_name);
  if (!service) {
    UROS_PRINT("Failed to find service: '%s'\n", service_name);
    return nullptr;
  }

  // add server via service, cause server is owned by service
  auto srv = service->addServer(cb);
  CHECK(srv);

  srv->setupEvent(&evt_, subs_.size());
  wait_set_ |= srv->bitMask();

  return static_cast<Server *>(subs_.emplace_back(srv));
}

inline void Node::spin() {
  while (true) {
    spinOnce();
  }
}

inline void Node::spinOnce(int timeout_ms) {
  auto flags = evt_.wait(wait_set_, true, false, timeout_ms);

  // new event may set when program reach here, the new topic data will
  // processed by the logic below,but event bit is not cleared, so wait will
  // successed and return immeidiately next turn, which would bring
  // redundant data. in case of this situation, generation should be checked
  // in spinOnce

  // process subscriptions (and servers)
  for (auto i = 0u; i < subs_.size(); ++i) {
    if (flags & (1 << i)) {
      subs_[i]->spinOnce();
    }
  }
}

} // namespace uros
