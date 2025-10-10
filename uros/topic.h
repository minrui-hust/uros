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
  TopicBase(const char *name, int32_t id, type_id_t msg_type, size_t msg_size)
      : id_(id), name_(name), msg_type_(msg_type), msg_size_(msg_size) {}

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

  virtual void setTransport(const int32_t tsp_id, TransportInterface *tsp) = 0;

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

  TopicT(const char *name, int32_t id)
      : TopicBase(name, id, type_id<TMsg>(), sizeof(TMsg)) {}

  void write(const TMsg &msg);

  bool read(TMsg &msg, int32_t &gen);

  void update(const TMsg &msg);

  // this should be non-blocking
  void recv(const int32_t tsp_id, const MsgBase *msg) override;

  void setTransport(const int32_t tsp_id, TransportInterface *tsp) override;

protected:
  TMsg msg_;

  etl::array<TransportInterface *, TransportManager::size> transports_{};
};

struct TopicManager {
  static TopicManager &Instance() {
    static TopicManager inst;
    return inst;
  }

  template <typename Topic>
  static Topic *AddTopic(const char *name, int32_t id) {
    return Instance().addTopic<Topic>(name, id);
  }

  template <typename Topic> static Topic *FindTopic(const char *name) {
    return Instance().findTopic<Topic>(name);
  }

protected:
  template <typename Topic> Topic *addTopic(const char *name, int32_t id);

  template <typename Topic> Topic *findTopic(const char *name);

protected:
  TopicManager() = default;
  TopicManager(const TopicManager &other) = delete;
  TopicManager &operator=(const TopicManager &other) = delete;

protected:
  etl::array<std::unique_ptr<TopicBase>, UROS_MAX_TOPICS> topics_{};
};

} // namespace uros
