#pragma once

#include "etl/memory.h"

#include "msg.h"
#include "subscription.h"
#include "system.h"

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

  ServerT(int id) : ServerBase(id) {
    sbc_.__meta__.type = MsgTypeServiceBroadcast;
    sbc_.__meta__.sys = System::Id();
    sbc_.__meta__.id.sbc.sys_from = System::Id();
    sbc_.__meta__.id.sbc.dist = 0;

    rsp_.__meta__.type = MsgTypeResponse;
  }

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
