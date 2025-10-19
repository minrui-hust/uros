#pragma once

#include "subscription.h"

namespace uros {

template <typename TReq, typename TRsp> struct ServiceT;

struct ServerBase : SubscriptionBase {
  ServerBase(int id) : SubscriptionBase(id) {}

  virtual ~ServerBase() = default;
};

template <typename TReq, typename TRsp> struct ServerT : ServerBase {
  using Req = TReq;
  using Rsp = TRsp;
  using Service = ServiceT<TReq, TRsp>;
  using ServiceCallback = typename Service::ServiceCallback;

  ServerT(int id) : ServerBase(id) {}

  bool bind(Service *service, const ServiceCallback &cb);

  void spinOnce() override;

protected:
  Req req_;
  Rsp rsp_;
  Service *service_;
  ServiceCallback cb_;
};

} // namespace uros
