
#include "server.h"
#include "service.h"

namespace uros {

template <typename TReq, typename TRsp>
bool ServerT<TReq, TRsp>::bind(
    Service *service, const std::function<void(const TReq &, TRsp &)> &cb) {
  cb_ = cb;
  service_ = service;
}

template <typename TReq, typename TRsp> void ServerT<TReq, TRsp>::spinOnce() {
  if (service_->readReq(req_, seq_)) {
    rsp_.__meta__.type = MsgTypeResponse;
    rsp_.__meta__.id.rsp.system = req_.__meta__.id.req.system;
    rsp_.__meta__.id.rsp.service = req_.__meta__.id.req.service;
    rsp_.__meta__.id.rsp.client = req_.__meta__.id.req.client;
    rsp_.__meta__.id.rsp.dist = 0;
    cb_(req_, rsp_);
    service_->writeRsp(rsp_, nullptr);
  }
}

} // namespace uros
