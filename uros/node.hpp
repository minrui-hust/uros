#pragma once

#include "node.h"

#include "publisher.h"
#include "service.h"
#include "subscription.h"
#include "topic.h"

// ============================================================
// node.hpp — Node 实现
// ============================================================

namespace uros {

// createSubscription(): 创建话题订阅。
// 流程：
//   1. 在 TopicManager 查找对应 Topic（类型和名字均匹配）
//   2. 通过 Topic 创建 SubscriptionT，Topic 持有其生命周期
//   3. 调用 setupEvent() 为该订阅分配事件位（bit_idx = subs_ 当前大小）
//   4. 将裸指针加入 subs_ 以便 spinOnce() 统一管理
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

// createPublisher(): 创建话题发布者。
// 流程：
//   1. 在 TopicManager 查找对应 Topic
//   2. 通过 Topic 创建 PublisherT，Topic 持有其生命周期
//   3. 将裸指针加入 pubs_（发布者不参与事件驱动，不需要事件位）
template <typename Msg>
PublisherT<Msg> *Node::createPublisher(const char *topic_name) {
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

// createClient(): 创建服务客户端。
// 流程：
//   1. 在 ServiceManager 查找对应 Service（类型和名字均匹配）
//   2. 通过 Service 创建 ClientT，Service 持有其生命周期
//   3. 将裸指针加入 clis_（客户端是主动发起方，不需要事件位）
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

// createServer(): 创建服务服务端。
// 流程：
//   1. 在 ServiceManager 查找对应 Service
//   2. 通过 Service 创建 ServerT，Service 持有其生命周期
//   3. 调用 setupEvent() 为该服务端分配事件位（复用 subs_ 的位置）
//   4. 将裸指针加入 subs_（Server 继承 SubscriptionBase，与订阅统一管理）
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

// spin(): 无限循环，持续调用 spinOnce()，通常运行在专用任务/线程中
inline void Node::spin() {
  while (true) {
    spinOnce();
  }
}

// spinOnce(): 等待任意 Subscription/Server 就绪，然后驱动其执行一次回调。
//
// 注意：EventGroup::wait() 返回后，可能已有新消息到达并置位事件，
// 但这批新消息会在下一轮 spinOnce() 中被处理（版本号机制保证不丢失）。
// 即使事件位被提前清除（clear_on_exit=true），版本比较也能正确检测到新数据。
inline void Node::spinOnce(int timeout_ms) {
  if (wait_set_ == 0) {
    msleep(timeout_ms);
    return;
  }

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
