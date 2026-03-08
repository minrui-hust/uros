#pragma once

#include <cstdint>

// ============================================================
// client.h — 服务客户端（Client）定义
//
// Client 是服务（Service）调用发起端的句柄。
// 用户通过 Node::createClient<Req, Rsp>(service_name) 获取，
// 之后调用 call(req, rsp, timeout_ms) 发起请求并等待应答。
//
// 设计要点：
//   - Client 由 Service 持有（通过 addClient() 创建），生命周期由 Service 管理。
//   - id_ 是该 Client 在所属 Service 的 clis_ 列表中的索引，
//     同时作为应答匹配的 client 字段写入请求消息。
//   - seq_ 单调递增，每次调用 +1，用于区分同一客户端的多次调用。
//   - 如果服务器在同一节点，走 localCall 路径（锁+版本信号量）；
//     否则走 remoteCall 路径（Transport 转发 + 等待应答）。
// ============================================================

namespace uros {

template <typename TReq, typename TRsp> struct ServiceT;

// ClientBase: 所有具体客户端的基类
struct ClientBase {
  ClientBase(int id) : id_(id) {}

  auto &id() const { return id_; }

  virtual ~ClientBase() = default;

protected:
  int id_; // 在 service.clis_ 数组中的下标，也用于应答匹配
};

// ClientT<TReq, TRsp>: 模板化客户端，绑定到特定请求/应答类型的 ServiceT
template <typename TReq, typename TRsp> struct ClientT : ClientBase {
  using Service = ServiceT<TReq, TRsp>;

  ClientT(int id) : ClientBase(id) {}

  // connect(): 将自身绑定到 Service，由 Service::addClient() 调用
  void connect(Service *service);

  // call(): 发起服务调用，填写元数据后委托给 Service::call()
  // timeout_ms < 0 表示永久等待
  bool call(const TReq &req, TRsp &rsp, int timeout_ms);

protected:
  Service *service_; // 关联的 Service 指针
  int seq_ = 0;      // 单调递增调用序号
};

} // namespace uros
