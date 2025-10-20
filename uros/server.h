#pragma once

#include "etl/memory.h"

#include "msg.h"
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

  void bind(Service *service, const ServiceCallback &cb);

  void spinOnce() override;

protected:
  void announce();

protected:
  Req req_;
  Rsp rsp_;
  Service *service_;
  ServiceCallback cb_;

  MsgBase sbc_;
  int sbc_seq_ = 0;

  etl::unique_ptr<Timer> announce_timer_{nullptr};
};

} // namespace uros
