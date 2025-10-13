#pragma once

#include "subscription.h"

namespace uros {

template <typename TReq, typename TRsp> struct ServiceT;

struct ServerBase : SubscriptionBase {};

template <typename TReq, typename TRsp> struct ServerT : ServerBase {
  using Service = ServiceT<TReq, TRsp>;

  bool serve(Service *service,
             const std::function<void(const TReq &, TRsp &)> &cb);

  void spinOnce() override;

protected:
  int id_; // TODO:
  Service *service_;
  std::function<void(const TReq &, TRsp &)> cb_;
};

} // namespace uros
