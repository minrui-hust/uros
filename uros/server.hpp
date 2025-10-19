
#include "server.h"
#include "service.h"

namespace uros {

template <typename TReq, typename TRsp>
bool ServerT<TReq, TRsp>::bind(Service *service, const ServiceCallback &cb) {
  cb_ = cb;
  service_ = service;
}

template <typename TReq, typename TRsp> void ServerT<TReq, TRsp>::spinOnce() {
  if (service_->readReq(req_, seq_)) {
    rsp_.__meta__.type = MsgTypeResponse;
    rsp_.__meta__.id.rsp = req_.__meta__.id.req;
    cb_(req_, rsp_);
    service_->writeRsp(this, rsp_);
  }
}

} // namespace uros
