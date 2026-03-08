
#include "server.h"
#include "service.h"
#include "system.h"

// ============================================================
// server.hpp — ServerT 实现
// ============================================================

namespace uros {

// bind(): 将 Server 与 Service 及回调绑定。
// 同时填写 ServiceBroadcast 中的 entry_hash 和 sys_src，
// 并启动 2s 定时器以周期性向外广播服务存在性。
template <typename TReq, typename TRsp>
void ServerT<TReq, TRsp>::bind(Service *service, const ServiceCallback &cb) {
  cb_ = cb;
  service_ = service;

  // fill sbc
  sbc_.__meta__.entry_hash = service_->id();
  sbc_.__meta__.sys_src = System::Id();

  // register a 2s timer task to periodically announce server existence
  announce_timer_ =
      etl::unique_ptr<Timer>(new Timer(2000, [&]() { announce(); }));
}

// announce(): 递增广播序号并通过 Service::writeServiceBroadcast() 向外广播。
// Service 会将广播转发给所有已注册的传输层（除广播来源外），
// 传输层收到后递增 dist 并继续转发，实现距离向量路由信息扩散。
template <typename TReq, typename TRsp> void ServerT<TReq, TRsp>::announce() {
  sbc_.seq = sbc_seq_++;
  service_->writeServiceBroadcast(this, sbc_);
}

// spinOnce(): 由 Node 在对应事件位触发时调用。
// 流程：
//   1. readReq() 以版本比较方式读取最新请求（有新请求才处理）
//   2. 填写应答消息元数据（sys_src, sys_dst, entry_hash, client, seq）
//   3. 调用用户回调 cb_(req_, rsp_)
//   4. writeRsp() 将应答写回 Service，由 Service 决定本地投递或通过传输层发送
template <typename TReq, typename TRsp> void ServerT<TReq, TRsp>::spinOnce() {
  UROS_PRINT("server '%d' spinOnce\n", id_);
  if (service_->readReq(req_, version_)) {
    rsp_.__meta__.sys_src = System::Id();
    rsp_.__meta__.sys_dst = req_.__meta__.sys_src;
    rsp_.__meta__.entry_hash = req_.__meta__.entry_hash;
    rsp_.client = req_.client;
    rsp_.seq = req_.seq;
    cb_(req_, rsp_);
    service_->writeRsp(this, rsp_);
  }
}

} // namespace uros
