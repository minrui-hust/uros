#pragma once

#include "subscription.h"

namespace uros {

template <typename TransportManager, typename TReq, typename TRsp>
struct ServiceT;

struct ClientBase {};

template <typename TransportManager, typename TReq, typename TRsp>
struct ClientT : ClientBase {
  using Service = ServiceT<TransportManager, TReq, TRsp>;

  void connect(Service *service);

  bool call(const TReq &req, TRsp &rsp, int timeout_ms);

protected:
  Service *service_;
  std::function<void(const TReq &, TRsp &)> cb_;
};

} // namespace uros
