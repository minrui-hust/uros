#pragma once

#include <cstring>

#include "etl/murmur3.h"
#include "etl/vector.h"

#include "platform.h"

#include "msg.h"
#include "publisher.h"
#include "subscription.h"
#include "utils.h"

// ============================================================
// topic.h — 话题（Topic）管理
//
// Topic 是 pub/sub 通信的核心对象，代表一条具有名字和消息类型的
// 数据通道。每个 Topic 存储一份最新消息（最后写入值语义），
// 当有新消息时通知所有本地订阅者和已注册的传输层。
//
// 层次关系：
//   TopicBase         — 非模板基类，持有元数据和多态接口
//   TopicT<TMsg>      — 模板子类，持有具体消息缓冲和类型化操作
//   TopicManager      — 全局单例，统一管理所有 Topic 的创建与查找
//
// pub/sub 数据流：
//   Publisher::publish()
//     → TopicT::write(pub, msg)   更新本地缓存，通知订阅者和传输层
//     → SubscriptionBase::notify() 设置 Node 事件位
//     → Node::spinOnce()          读取新消息并调用用户回调
//
// 跨节点数据流（经过 Transport）：
//   TopicT::notify(tsp=nullptr 或本地) → TransportBase::notify()
//     → 发送线程将最新消息序列化发送
//   TransportBase 收到远端消息 → TopicT::write(tsp, rawMsg)
//     → 同上，通知本地订阅者和其他传输层（不含来源传输层，防止回路）
// ============================================================

namespace uros {

struct TransportBase;

// TopicBase: 非模板基类，存储话题的公共属性和纯虚接口
struct TopicBase {
  TopicBase(const char *name, int prio, type_id_t msg_type, size_t msg_size)
      : name_(name), prio_(prio), msg_type_(msg_type), msg_size_(msg_size) {
    id_ = calc_entry_hash(name); // 用名字的 murmur3 哈希作为全局唯一 ID
  }

  // 属性访问器
  const auto &id() const { return id_; }
  const char *name() const { return name_; }
  const auto &prio() const { return prio_; }
  const type_id_t &msgType() const { return msg_type_; }
  const size_t &msgSize() const { return msg_size_; }

  void setPrio(int prio) { prio_ = prio; }

  // 将传输层注册到本话题（一个 Topic 可关联多个传输层）
  bool registerTransport(TransportBase *tsp);

  // 纯虚接口：由 TransportBase 调用，以原始指针方式写入/读取消息
  virtual void write(TransportBase *tsp, const MsgBase *msg) = 0;
  virtual size_t read(MsgBase *msg, int &seq) = 0;

  virtual ~TopicBase() = default;

protected:
  uint32_t id_;        // 话题的全局唯一 ID（名字哈希）
  const char *name_;   // 话题名称字符串
  int prio_ = -1;      // 消息传输优先级
  type_id_t msg_type_; // 消息类型 ID（用于类型安全检查）
  size_t msg_size_;    // 消息总大小（含 MsgBase 头部）
  int version_ = -1;   // 当前消息版本号（每次写入 +1）

  // 该话题下所有发布者（由 addPublisher() 创建，生命周期由 Topic 管理）
  etl::vector<etl::unique_ptr<PublisherBase>, UROS_TOPIC_MAX_PUBS> pubs_;
  // 该话题下所有订阅者（由 addSubscription() 创建，生命周期由 Topic 管理）
  etl::vector<etl::unique_ptr<SubscriptionBase>, UROS_TOPIC_MAX_SUBS> subs_;

  // 已注册的传输层指针（下标对应 Transport::id_），空位为 nullptr
  etl::array<TransportBase *, UROS_MAX_TRANSPORTS> tsps_{};
};

// TopicT<TMsg>: 模板化话题，持有类型化消息缓冲和操作
template <typename TMsg> struct TopicT : public TopicBase {
  using Msg = TMsg;
  using Publisher = PublisherT<TMsg>;
  using Subscription = SubscriptionT<TMsg>;

  TopicT(const char *name, int prio)
      : TopicBase(name, prio, type_id<TMsg>(), sizeof(TMsg)) {}

  // 创建一个新发布者并注册到本话题
  PublisherT<Msg> *addPublisher();

  // 创建一个新订阅者并注册到本话题
  SubscriptionT<Msg> *
  addSubscription(const std::function<void(const Msg &)> &cb);

  // 由本地 Publisher 写入消息（来源 tsp 为 nullptr，不回传给任何传输层）
  void write(Publisher *pub, const TMsg &msg);
  // 由 Transport 写入远端消息（来源 tsp 非 nullptr，通知时跳过该传输层）
  void write(TransportBase *tsp, const MsgBase *msg) override;

  // 读取最新消息：若 topic version > gen 则更新 msg 和 gen 并返回长度，否则返回 0
  size_t read(TMsg &msg, int &gen);
  size_t read(MsgBase *msg, int &gen) override;

protected:
  // update(): 在临界区内原子地更新本地消息缓冲并递增版本号
  bool update(const TMsg &msg);
  // notify(): 通知所有本地订阅者（设置事件位）和已注册的传输层（触发发送）
  void notify(TransportBase *tsp);

protected:
  TMsg msg_; // 最新消息的本地缓冲（最后写入值语义）
};

// TopicManager: 全局单例，负责所有 Topic 的创建和查找
// 使用方式：
//   auto topic = TopicManager::AddTopic<TopicT<MyMsg>>("my_topic");
//   auto topic = TopicManager::FindTopic<TopicT<MyMsg>>("my_topic");
struct TopicManager {

  // AddTopic: 创建并注册一个新话题；若 Topic 已满则返回 nullptr
  template <typename Topic>
  static Topic *AddTopic(const char *name, int prio = 0) {
    return Instance().addTopic<Topic>(name);
  }

  // FindTopic: 按名称查找话题，同时检查消息类型匹配
  template <typename Topic> static Topic *FindTopic(const char *name) {
    return Instance().findTopic<Topic>(name);
  }

protected:
  static TopicManager &Instance() {
    static TopicManager inst;
    return inst;
  }

  template <typename Topic> Topic *addTopic(const char *name, int prio = 0);

  template <typename Topic> Topic *findTopic(const char *name);

protected:
  TopicManager() = default;
  TopicManager(const TopicManager &other) = delete;
  TopicManager &operator=(const TopicManager &other) = delete;

protected:
  // 所有已创建的 Topic，以 unique_ptr 持有（静态上限 UROS_MAX_TOPICS）
  etl::vector<etl::unique_ptr<TopicBase>, UROS_MAX_TOPICS> topics_;
};

} // namespace uros
