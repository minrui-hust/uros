#pragma once

#include "etl/vector.h"

#include "platform.h"

namespace uros {

// topic related pre-declaration
struct SubscriptionBase;
template <typename TransportManager, typename TMsg> struct SubscriptionT;

struct PublisherBase;
template <typename TransportManager, typename TMsg> struct PublisherT;

struct TopicBase;
template <typename TransportManager, typename TMsg> struct TopicT;

// service related pre-declaration
struct ClientBase;
template <typename TransportManager, typename TReq, typename TRsp>
struct ClientT;

struct ServerBase;
template <typename TransportManager, typename TReq, typename TRsp>
struct ServerT;

struct ServiceBase;
template <typename TransportManager, typename TReq, typename TRsp>
struct ServiceT;

template <typename TransportManager> struct NodeT {

  // topic related api
  template <typename TMsg>
  SubscriptionT<TransportManager, TMsg> *
  createSubscription(const char *topic_name,
                     const std::function<void(const TMsg &)> &cb);

  template <typename TMsg>
  PublisherT<TransportManager, TMsg> *createPublisher(const char *topic_name);

  // service related api
  template <typename TReq, typename TRsp>
  ServerT<TransportManager, TReq, TRsp> *
  createServer(const char *service_name,
               const std::function<void(const TReq &, TRsp &)> &cb);

  template <typename TReq, typename TRsp>
  ClientT<TransportManager, TReq, TRsp> *createClient(const char *service_name);

  void spin();
  void spinOnce(int32_t timeout_ms = -1);

protected:
  EventGroup evt_;
  EventBits wait_set_ = 0;

  etl::vector<std::unique_ptr<SubscriptionBase>, UROS_NODE_MAX_SUBS> subs_;
  etl::vector<std::unique_ptr<PublisherBase>, UROS_NODE_MAX_PUBS> pubs_;
  etl::vector<std::unique_ptr<ClientBase>, UROS_NODE_MAX_CLIS> clis_;
};

} // namespace uros
