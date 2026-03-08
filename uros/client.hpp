#pragma once

#include "client.h"
#include "service.h"
#include "system.h"

// ============================================================
// client.hpp — ClientT 实现
// ============================================================

namespace uros {

// connect(): 保存 Service 指针，由 Service::addClient() 调用
template <typename TReq, typename TRsp>
void ClientT<TReq, TRsp>::connect(Service *service) {
  service_ = service;
}

// call(): 在调用服务前由中间件自动填写请求元数据：
//   - type       = MsgTypeRequest
//   - sys_src    = 本节点 ID（应答时作为 sys_dst 校验）
//   - entry_hash = 所属服务的哈希 ID
//   - client     = 本 Client 在 Service 中的索引
//   - seq        = 单调递增序号（每次调用 +1）
// 填写完毕后委托给 Service::call() 决定走本地还是远端路径
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
