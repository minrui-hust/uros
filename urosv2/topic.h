#pragma once

#include "etl/vector.h"

#include "platform.h"

#include "msg.h"
#include "publisher.h"
#include "subscription.h"
#include "utils.h"

namespace uros {

struct TransportBase;

struct TopicBase {
  TopicBase(const char *name, int id, int prio, type_id_t msg_type,
            size_t msg_size)
      : id_(id), name_(name), prio_(prio), msg_type_(msg_type),
        msg_size_(msg_size) {}

  const auto &id() const { return id_; }

  const char *name() const { return name_; }

  const auto &prio() const { return prio_; }

  const type_id_t &msgType() const { return msg_type_; }

  const size_t &msgSize() const { return msg_size_; }

  bool addTransport(TransportBase *tsp);

  virtual bool read(MsgBase *msg, int16_t &seq) = 0;
  virtual int16_t write(const MsgBase *msg,
                        TransportBase *from_tsp = nullptr) = 0;

  virtual ~TopicBase() = default;

protected:
  int id_; // global unique identification of a topic
  const char *name_;
  int prio_;
  type_id_t msg_type_;
  size_t msg_size_;

  etl::vector<etl::unique_ptr<PublisherBase>, UROS_TOPIC_MAX_PUBS> pubs_;
  etl::vector<etl::unique_ptr<SubscriptionBase>, UROS_TOPIC_MAX_SUBS> subs_;
  etl::vector<TransportBase *, UROS_MAX_TRANSPORTS> tsps_;
};

template <typename TMsg> struct TopicT : public TopicBase {
  using Msg = TMsg;

  TopicT(const char *name, int id, int prio)
      : TopicBase(name, id, prio, type_id<TMsg>(), sizeof(TMsg)) {
    msg_.__meta__.id.msg.seq = -1;
  }

  PublisherT<Msg> *addPublisher();

  SubscriptionT<Msg> *
  addSubscription(const std::function<void(const Msg &)> &cb);

  // write new msg on topic
  int16_t write(const TMsg &msg, TransportBase *from_tsp = nullptr);
  int16_t write(const MsgBase *msg, TransportBase *from_tsp = nullptr) override;

  // read msg on topic
  bool read(TMsg &msg, int16_t &gen);
  bool read(MsgBase *msg, int16_t &gen) override;

protected:
  TMsg msg_;
};

struct TopicManager {

  template <typename Topic>
  static Topic *AddTopic(const char *name, int id, int prio) {
    return Instance().addTopic<Topic>(name, id, prio);
  }

  template <typename Topic> static Topic *FindTopic(const char *name) {
    return Instance().findTopic<Topic>(name);
  }

protected:
  static TopicManager &Instance() {
    static TopicManager inst;
    return inst;
  }

  template <typename Topic>
  Topic *addTopic(const char *name, uint8_t id, uint8_t prio);

  template <typename Topic> Topic *findTopic(const char *name);

protected:
  TopicManager() = default;
  TopicManager(const TopicManager &other) = delete;
  TopicManager &operator=(const TopicManager &other) = delete;

protected:
  etl::array<etl::unique_ptr<TopicBase>, UROS_MAX_TOPICS> topics_{};
};

} // namespace uros
