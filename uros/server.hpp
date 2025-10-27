
#include "server.h"
#include "service.h"
#include "system.h"

namespace uros {

template <typename TReq, typename TRsp>
void ServerT<TReq, TRsp>::bind(Service *service, const ServiceCallback &cb) {
  cb_ = cb;
  service_ = service;

  // fill sbc
  sbc_.__meta__.id.sbc.service = service_->id();

  // register a 2s timer task to periodically announce server existence
  announce_timer_ =
      etl::unique_ptr<Timer>(new Timer(2000, [&]() { announce(); }));
}

template <typename TReq, typename TRsp> void ServerT<TReq, TRsp>::announce() {
  sbc_.__meta__.id.sbc.seq = sbc_seq_++;
  service_->writeServiceBroadcast(this, sbc_);
}

template <typename TReq, typename TRsp> void ServerT<TReq, TRsp>::spinOnce() {
  UROS_PRINT("server '%d' spinOnce\n", id_);
  if (service_->readReq(req_, version_)) {
    rsp_.__meta__.sys = req_.__meta__.sys;
    rsp_.__meta__.id.rsp = req_.__meta__.id.req;
    cb_(req_, rsp_);
    service_->writeRsp(this, rsp_);
  }
}

} // namespace uros
