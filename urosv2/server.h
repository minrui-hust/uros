#pragma once

#include "subscription.h"

namespace uros {

template <typename TReq, typename TRsp> struct ServiceT;

struct ServerBase : SubscriptionBase {
  ServerBase(int id) : SubscriptionBase(id) {}

  virtual ~ServerBase() = default;
};

template <typename TReq, typename TRsp> struct ServerT : ServerBase {
  using Service = ServiceT<TReq, TRsp>;

  ServerT(int id) : ServerBase(id) {}

  bool bind(Service *service,
            const std::function<void(const TReq &, TRsp &)> &cb);

  void spinOnce() override;

protected:
  TReq req_;
  TRsp rsp_;
  Service *service_;
  std::function<void(const TReq &, TRsp &)> cb_;
};

} // namespace uros
