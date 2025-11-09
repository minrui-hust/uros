#pragma once

#include "client.h"
#include "service.h"
#include "system.h"

namespace uros {

template <typename TReq, typename TRsp>
void ClientT<TReq, TRsp>::connect(Service *service) {
  service_ = service;
}

template <typename TReq, typename TRsp>
bool ClientT<TReq, TRsp>::call(const TReq &req, TRsp &rsp, int timeout_ms) {
  req.__meta__.type = MsgTypeRequest;
  req.__meta__.sys_src = System::Id();
  req.__meta__.entry_hash = service_->id();
  req.client = this->id();
  req.seq = seq_++;
  return service_->call(this, req, rsp, timeout_ms);
}

} // namespace uros
