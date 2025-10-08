#pragma once

#include "etl/vector.h"

#include "uros_config.h"

#include "msg.h"
#include "publisher.h"
#include "subscription.h"
#include "transport.h"
#include "utils.h"

namespace uros {

struct TopicBase {
  TopicBase(const char *name, type_id_t msg_type, size_t msg_size)
      : name_(name), msg_type_(msg_type), msg_size_(msg_size) {}

  int32_t &id() { return id_; }
  const int32_t &id() const { return id_; }

  const char *name() const { return name_; }

  const type_id_t &msgType() const { return msg_type_; }

  const size_t &msgSize() const { return msg_size_; }

  int32_t generation() const { return generation_; }

  bool registerSubscription(SubscriptionBase *sub) {
    if (subs_.full()) {
      return false;
    }
    subs_.emplace_back(sub);
    return true;
  }

  bool registerPublisher(PublisherBase *pub) {
    if (pubs_.full()) {
      return false;
    }
    pubs_.emplace_back(pub);
    return true;
  }

  // this should be non-blocking
  virtual void recv(const int32_t tsp_id, const MsgBase *msg) = 0;

protected:
  int32_t id_; // global unique identification of a topic
  const char *name_;
  type_id_t msg_type_;
  size_t msg_size_;
  int32_t generation_ = -1;

  etl::vector<SubscriptionBase *, UROS_TOPIC_MAX_SUBS> subs_;
  etl::vector<PublisherBase *, UROS_TOPIC_MAX_PUBS> pubs_;
};

template <typename TTransportManager, typename TMsg>
struct TopicT : public TopicBase {
  using TransportManager = TTransportManager;
  using Msg = TMsg;

  TopicT(const char *name) : TopicBase(name, type_id<TMsg>(), sizeof(TMsg)) {}

  template <size_t Idx, typename Transport>
  void setTransport(Transport *transport);

  void write(const TMsg &msg);

  bool read(TMsg &msg, int32_t &gen);

  void update(const TMsg &msg);

  // this should be non-blocking
  void recv(const int32_t tsp_id, const MsgBase *msg) override;

protected:
  TMsg msg_;

  template <typename Transport> struct TransportInfo {
    using type = Transport *;
  };

  using TransportTuple =
      typename TransportManager::template ApplyTransports<TransportInfo,
                                                          std::tuple>;

  TransportTuple transports_{}; // init to nullptr
};

struct TopicManager {
  static TopicManager &Instance() {
    static TopicManager inst;
    return inst;
  }

  template <typename Topic> static Topic *FindOrAdd(const char *name) {
    return Instance().findOrAdd<Topic>(name);
  }

protected:
  template <typename Topic> Topic *findOrAdd(const char *name);

protected:
  TopicManager() = default;
  TopicManager(const TopicManager &other) = delete;
  TopicManager &operator=(const TopicManager &other) = delete;

protected:
  etl::vector<std::unique_ptr<TopicBase>, UROS_MAX_TOPICS> topics_;
};

} // namespace uros
