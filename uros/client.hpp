#pragma once

#include "client.h"

namespace uros {

template <typename TransportManager, typename TReq, typename TRsp>
void ClientT<TransportManager, TReq, TRsp>::connect(
    ServiceT<TransportManager, TReq, TRsp> *service) {
  service_ = service;
}

template <typename TransportManager, typename TReq, typename TRsp>
bool ClientT<TransportManager, TReq, TRsp>::call(const TReq &req, TRsp &rsp,
                                                 int timeout_ms) {
  sendRequest(req);
  return waitResponse(rsp, timeout_ms);
}

} // namespace uros
