#pragma once

#include "client.h"
#include "service.h"

namespace uros {

template <typename TReq, typename TRsp>
void ClientT<TReq, TRsp>::connect(Service *service) {
  service_ = service;
}

template <typename TReq, typename TRsp>
bool ClientT<TReq, TRsp>::call(const TReq &req, TRsp &rsp, int timeout_ms) {
  req.__meta__.type = MsgTypeRequest;
  req.__meta__.id.req.client = id_;
  return service_->call(req, rsp, timeout_ms);
}

} // namespace uros
