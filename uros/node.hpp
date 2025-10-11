#pragma once

#include "node.h"

#include "publisher.h"
#include "server.h"
#include "subscription.h"
#include "topic.h"

namespace uros {

template <typename TransportManager>
template <typename TMsg>
SubscriptionT<TransportManager, TMsg> *
NodeT<TransportManager>::createSubscription(
    const char *topic_name, const std::function<void(const TMsg &)> &cb) {
  using Topic = TopicT<TransportManager, TMsg>;
  using Subscription = SubscriptionT<TransportManager, TMsg>;

  if (subs_.full()) {
    return nullptr;
  }

  auto topic = TopicManager::FindTopic<Topic>(topic_name);
  if (!topic) {
    UROS_PRINT("Failed to find topic: '%s'\n", topic_name);
    return nullptr;
  }

  auto sub = std::make_unique<Subscription>(&evt_, subs_.size());
  CHECK(sub);

  sub->subscribe(topic, cb);

  if (!topic->registerSubscription(sub.get())) {
    return nullptr;
  }

  wait_set_ |= sub->bitMask();

  return static_cast<Subscription *>(subs_.emplace_back(std::move(sub)).get());
}

template <typename TransportManager>
template <typename TMsg>
PublisherT<TransportManager, TMsg> *
NodeT<TransportManager>::createPublisher(const char *topic_name) {
  using Topic = TopicT<TransportManager, TMsg>;
  using Publisher = PublisherT<TransportManager, TMsg>;

  if (pubs_.full()) {
    return nullptr;
  }

  auto topic = TopicManager::FindTopic<Topic>(topic_name);
  if (!topic) {
    UROS_PRINT("Failed to find topic: '%s'\n", topic_name);
    return nullptr;
  }

  auto pub = std::make_unique<Publisher>();
  CHECK(pub);

  pub->advertise(topic);

  if (!topic->registerPublisher(pub.get())) {
    return nullptr;
  }

  return static_cast<Publisher *>(pubs_.emplace_back(std::move(pub)).get());
}

template <typename TransportManager>
template <typename TReq, typename TRsp>
ClientT<TransportManager, TReq, TRsp> *
NodeT<TransportManager>::createClient(const char *service_name) {
  using Service = ServiceT<TransportManager, TReq, TRsp>;
  using Client = ClientT<TransportManager, TRsp, TReq>;

  if (clis_.full()) {
    return nullptr;
  }

  auto service = ServiceManager::FindService<Service>(service_name);
  if (!service) {
    UROS_PRINT("Failed to find service: '%s'\n", service_name);
    return nullptr;
  }

  auto cli = std::make_unique<Client>();
  CHECK(cli);

  cli->connect(service);

  if (!service->registerClient(cli.get())) {
    return nullptr;
  }

  return static_cast<Client *>(clis_.emplace_back(std::move(cli)).get());
}

template <typename TransportManager>
template <typename TReq, typename TRsp>
ServerT<TransportManager, TReq, TRsp> *NodeT<TransportManager>::createServer(
    const char *service_name,
    const std::function<void(const TReq &, TRsp &)> &cb) {
  using Service = ServiceT<TransportManager, TReq, TRsp>;
  using Server = ServerT<TransportManager, TReq, TRsp>;

  if (subs_.full()) {
    return nullptr;
  }

  auto service = ServiceManager::FindService<Service>(service_name);
  if (!service) {
    UROS_PRINT("Failed to find service: '%s'\n", service_name);
    return nullptr;
  }

  auto srv = std::make_unique<Server>(&evt_, subs_.size());
  CHECK(srv);

  srv->serve(service, cb);

  if (!service->registerServer(srv->get())) {
    return nullptr;
  }

  wait_set_ |= srv->bitMask();

  return static_cast<Server *>(subs_.emplace_back(std::move(srv)).get());
}

template <typename TransportManager> void NodeT<TransportManager>::spin() {
  while (true) {
    spinOnce(-1);
  }
}

template <typename TransportManager>
void NodeT<TransportManager>::spinOnce(int32_t timeout_ms) {
  auto flags = evt_.wait(wait_set_, true, false, timeout_ms);

  // new event may set when program reach here, the new topic data will
  // processed by the logic below,but event bit is not cleared, so wait will
  // successed and return immeidiately next turn, which would bring
  // redundant data. in case of this situation, generation should be checked
  // in spinOnce

  // process subscriptions
  for (auto i = 0u; i < subs_.size(); ++i) {
    if (flags & (1 << i)) {
      subs_[i]->spinOnce();
    }
  }

  // TODO: process servers
  // for (auto i = 0u; i < srvs_.size(); ++i) {
  //   if (flags & (1 << (i + 16))) {
  //     srvs_[i]->spinOnce();
  //   }
  // }
}

} // namespace uros
