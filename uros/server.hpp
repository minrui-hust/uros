
#include "server.h"
#include "service.h"
#include "system.h"

namespace uros {

template <typename TReq, typename TRsp>
void ServerT<TReq, TRsp>::bind(Service *service, const ServiceCallback &cb) {
  cb_ = cb;
  service_ = service;

  // fill sbc
  sbc_.__meta__.type = MsgTypeServiceBroadcast;
  sbc_.__meta__.sys = System::Id();
  sbc_.__meta__.id.sbc.sys_from = System::Id();
  sbc_.__meta__.id.sbc.service = service_->id();
  sbc_.__meta__.id.sbc.dist = 0;

  // register a timer task to periodically announce server existence
  announce_timer_ =
      etl::unique_ptr<Timer>(new Timer(1000, [&]() { announce(); }));
}

template <typename TReq, typename TRsp> void ServerT<TReq, TRsp>::announce() {
  sbc_.__meta__.id.sbc.seq = sbc_seq_;
  service_->writeServiceBroadcast(this, sbc_);
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
