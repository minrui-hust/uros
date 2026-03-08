#pragma once

#include "etl/vector.h"

#include "platform.h"

// ============================================================
// node.h — 用户节点（Node）定义
//
// Node 是 uros 面向应用层的核心 API 对象，类似 ROS2 的 Node。
// 用户在一个 Node 上创建 Publisher、Subscription、Client、Server，
// 然后通过 spin() / spinOnce() 驱动回调执行。
//
// 内部机制：
//   - Node 使用一个 EventGroup（事件组）汇聚所有 Subscription/Server
//     的就绪通知，每个 Subscription/Server 占用一个事件位（bit）。
//   - spinOnce() 调用 EventGroup::wait() 等待任意位置位，
//     然后逐位检查并调用对应 Subscription/Server 的 spinOnce()。
//   - spin() 是 spinOnce() 的无限循环版本，阻塞当前线程。
//
// 所有权说明：
//   - Node 持有 Subscription/Server 的裸指针，
//     实际对象由 Topic/Service 通过 unique_ptr 持有，不可通过 Node 删除。
//   - Publisher/Client 同理。
// ============================================================

namespace uros {

// topic related pre-declaration
struct SubscriptionBase;
template <typename TMsg> struct SubscriptionT;

struct PublisherBase;
template <typename TMsg> struct PublisherT;

struct TopicBase;
template <typename TMsg> struct TopicT;

// service related pre-declaration
struct ClientBase;
template <typename TReq, typename TRsp> struct ClientT;

struct ServerBase;
template <typename TReq, typename TRsp> struct ServerT;

struct ServiceBase;
template <typename TReq, typename TRsp> struct ServiceT;

struct Node {

  // ---- 话题相关 API ----

  // createSubscription(): 查找名为 topic_name 的话题，创建订阅并注册回调。
  // 返回 nullptr 表示话题不存在或订阅数已满。
  template <typename TMsg>
  SubscriptionT<TMsg> *
  createSubscription(const char *topic_name,
                     const std::function<void(const TMsg &)> &cb);

  // createPublisher(): 查找名为 topic_name 的话题，创建发布者。
  // 返回 nullptr 表示话题不存在或发布者数已满。
  template <typename TMsg>
  PublisherT<TMsg> *createPublisher(const char *topic_name);

  // ---- 服务相关 API ----

  // createServer(): 查找名为 service_name 的服务，创建服务端并注册回调。
  // 返回 nullptr 表示服务不存在或服务端数已满。
  template <typename TReq, typename TRsp>
  ServerT<TReq, TRsp> *
  createServer(const char *service_name,
               const std::function<void(const TReq &, TRsp &)> &cb);

  // createClient(): 查找名为 service_name 的服务，创建客户端。
  // 返回 nullptr 表示服务不存在或客户端数已满。
  template <typename TReq, typename TRsp>
  ClientT<TReq, TRsp> *createClient(const char *service_name);

  // ---- 驱动循环 ----

  // spin(): 无限循环，持续调用 spinOnce()，阻塞当前线程
  void spin();

  // spinOnce(): 等待事件（任意 Subscription/Server 就绪），
  // 然后对所有置位的回调执行一次处理
  void spinOnce(int timeout_ms = -1);

protected:
  // subs_: 持有所有 Subscription 和 Server 的裸指针（两者均继承 SubscriptionBase）
  // 每个元素占 EventGroup 中的一个位（bit_idx = 在 subs_ 中的下标）
  etl::vector<SubscriptionBase *, UROS_NODE_MAX_SUBS> subs_;
  etl::vector<PublisherBase *, UROS_NODE_MAX_PUBS> pubs_;
  etl::vector<ClientBase *, UROS_NODE_MAX_CLIS> clis_;

  EventGroup evt_;       // 事件组：汇聚所有订阅/服务端的就绪通知
  uint32_t wait_set_ = 0; // 当前已注册的事件位掩码（OR 值）
};

} // namespace uros
