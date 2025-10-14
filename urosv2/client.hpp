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
  req.__id__.participant = id_;
  req.__id__.type = MsgTypeRequest;
  return service_->call(req, rsp, timeout_ms);
}

} // namespace uros
