#pragma once

#include "etl/memory.h"

#include "msg.h"
#include "subscription.h"
#include "system.h"

// ============================================================
// server.h — 服务服务端（Server）定义
//
// Server 是服务（Service）处理端的句柄，继承自 SubscriptionBase，
// 因此可以被 Node 统一管理（事件组唤醒、spinOnce 处理）。
//
// 设计要点：
//   - Server 由 Service 持有（通过 addServer() 创建），生命周期由 Service 管理。
//   - Server 持有一个周期定时器（announce_timer_，默认2s），
//     定时调用 announce() 向所有传输层广播 ServiceBroadcast，
//     以便其他节点发现本服务并更新路由距离。
//   - spinOnce() 由 Node 在对应事件位触发时调用：
//     读取最新请求 → 调用用户回调 cb_() → 写回应答
// ============================================================

namespace uros {

template <typename TReq, typename TRsp> struct ServiceT;

// ServerBase: 所有服务端的基类，复用 SubscriptionBase 的事件通知机制
struct ServerBase : SubscriptionBase {
  ServerBase(int id) : SubscriptionBase(id) {}

  virtual ~ServerBase() = default;
};

// ServerT<TReq, TRsp>: 模板化服务端，绑定到特定请求/应答类型的 ServiceT
template <typename TReq, typename TRsp> struct ServerT : ServerBase {
  using Req = TReq;
  using Rsp = TRsp;
  using Service = ServiceT<TReq, TRsp>;
  using ServiceCallback = typename Service::ServiceCallback;

  ServerT(int id) : ServerBase(id) {
    // 初始化服务广播消息元数据
    sbc_.__meta__.type = MsgTypeServiceBroadcast;
    sbc_.__meta__.sys_src = System::Id();
    sbc_.__meta__.sys_pre = System::Id();
    sbc_.dist = 0; // 本节点距离为 0

    rsp_.__meta__.type = MsgTypeResponse;
  }

  // bind(): 将 Server 与 Service 及用户回调绑定，并启动周期广播定时器
  void bind(Service *service, const ServiceCallback &cb);

  // spinOnce(): 尝试读取最新请求（版本更新才处理），调用回调，写回应答
  void spinOnce() override;

protected:
  // announce(): 递增广播序号并通过 Service 向所有传输层发送 ServiceBroadcast
  void announce();

protected:
  Req req_;                              // 请求缓冲
  Rsp rsp_;                              // 应答缓冲
  Service *service_;                     // 关联的 Service 指针
  ServiceCallback cb_;                   // 用户注册的服务处理回调

  ServiceBroadcast sbc_;                 // 服务广播消息（携带距离和序号）
  int sbc_seq_ = 0;                      // 广播序号，每次 announce() +1

  etl::unique_ptr<Timer> announce_timer_{nullptr}; // 周期广播定时器（默认2s）
};

} // namespace uros
