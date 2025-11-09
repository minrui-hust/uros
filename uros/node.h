#pragma once

#include "etl/vector.h"

#include "platform.h"

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

  // topic related api
  template <typename TMsg>
  SubscriptionT<TMsg> *
  createSubscription(const char *topic_name,
                     const std::function<void(const TMsg &)> &cb);

  template <typename TMsg>
  PublisherT<TMsg> *createPublisher(const char *topic_name);

  // service related api
  template <typename TReq, typename TRsp>
  ServerT<TReq, TRsp> *
  createServer(const char *service_name,
               const std::function<void(const TReq &, TRsp &)> &cb);

  template <typename TReq, typename TRsp>
  ClientT<TReq, TRsp> *createClient(const char *service_name);

  void spin();

  void spinOnce(int timeout_ms = -1);

protected:
  // contain both subsciption and server
  etl::vector<SubscriptionBase *, UROS_NODE_MAX_SUBS> subs_;
  etl::vector<PublisherBase *, UROS_NODE_MAX_PUBS> pubs_;
  etl::vector<ClientBase *, UROS_NODE_MAX_CLIS> clis_;

  EventGroup evt_;
  uint32_t wait_set_ = 0;
};

} // namespace uros
