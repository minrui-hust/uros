
#include "server.h"

namespace uros {

template <typename TReq, typename TRsp>
bool ServerT<TReq, TRsp>::bind(
    Service *service, const std::function<void(const TReq &, TRsp &)> &cb) {
  cb_ = cb;
  service_ = service;
}

template <typename TReq, typename TRsp> void ServerT<TReq, TRsp>::spinOnce() {
  cb_(*service_->req(), *service_->rsp());
  service_->notifyRsp();
}

} // namespace uros
