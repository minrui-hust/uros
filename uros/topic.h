#pragma once

#include <cstring>

#include "etl/murmur3.h"
#include "etl/vector.h"

#include "platform.h"

#include "msg.h"
#include "publisher.h"
#include "subscription.h"
#include "utils.h"

namespace uros {

struct TransportBase;

struct TopicBase {
  TopicBase(const char *name, int prio, type_id_t msg_type, size_t msg_size)
      : name_(name), prio_(prio), msg_type_(msg_type), msg_size_(msg_size) {
    id_ = calc_entry_hash(name);
  }

  // property accessor
  const auto &id() const { return id_; }
  const char *name() const { return name_; }
  const auto &prio() const { return prio_; }
  const type_id_t &msgType() const { return msg_type_; }
  const size_t &msgSize() const { return msg_size_; }

  void setPrio(int prio) { prio_ = prio; }

  bool registerTransport(TransportBase *tsp);

  virtual void write(TransportBase *tsp, const MsgBase *msg) = 0;
  virtual size_t read(MsgBase *msg, int &seq) = 0;

  virtual ~TopicBase() = default;

protected:
  uint32_t id_; // global unique identification of a topic
  const char *name_;
  int prio_ = -1;
  type_id_t msg_type_;
  size_t msg_size_;
  int version_ = -1;

  etl::vector<etl::unique_ptr<PublisherBase>, UROS_TOPIC_MAX_PUBS> pubs_;
  etl::vector<etl::unique_ptr<SubscriptionBase>, UROS_TOPIC_MAX_SUBS> subs_;

  etl::array<TransportBase *, UROS_MAX_TRANSPORTS> tsps_{};
};

template <typename TMsg> struct TopicT : public TopicBase {
  using Msg = TMsg;
  using Publisher = PublisherT<TMsg>;
  using Subscription = SubscriptionT<TMsg>;

  TopicT(const char *name, int prio)
      : TopicBase(name, prio, type_id<TMsg>(), sizeof(TMsg)) {}

  PublisherT<Msg> *addPublisher();

  SubscriptionT<Msg> *
  addSubscription(const std::function<void(const Msg &)> &cb);

  // write new msg on topic
  void write(Publisher *pub, const TMsg &msg);
  void write(TransportBase *tsp, const MsgBase *msg) override;

  // read msg on topic
  size_t read(TMsg &msg, int &gen);
  size_t read(MsgBase *msg, int &gen) override;

protected:
  bool update(const TMsg &msg);
  void notify(TransportBase *tsp);

protected:
  TMsg msg_;
};

struct TopicManager {

  template <typename Topic>
  static Topic *AddTopic(const char *name, int prio = 0) {
    return Instance().addTopic<Topic>(name);
  }

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
  etl::vector<etl::unique_ptr<TopicBase>, UROS_MAX_TOPICS> topics_;
};

} // namespace uros
