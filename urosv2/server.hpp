
#include "server.h"

namespace uros {

template <typename TReq, typename TRsp>
bool ServerT<TReq, TRsp>::bind(
    Service *service, const std::function<void(const TReq &, TRsp &)> &cb) {
  cb_ = cb;
  service_ = service;
}

template <typename TReq, typename TRsp> void ServerT<TReq, TRsp>::spinOnce() {
  if (service_->readReq(req_, generation_)) {
    cb_(req_, rsp_);
    service_->writeRsp(rsp_);
  }
}

} // namespace uros
