
#include "server.h"

namespace uros {

template <typename TransportManager, typename TReq, typename TRsp>
bool ServerT<TransportManager, TReq, TRsp>::serve(
    Service *service, const std::function<void(const TReq &, TRsp &)> &cb) {
  cb_ = cb;
  service_ = service;
}

template <typename TransportManager, typename TReq, typename TRsp>
void ServerT<TransportManager, TReq, TRsp>::spinOnce() {
  // TODO:
}

} // namespace uros
