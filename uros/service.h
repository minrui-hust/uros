#pragma once

#include <climits>
#include <cstring>

#include "etl/murmur3.h"
#include "etl/vector.h"

#include "platform.h"

#include "client.h"
#include "msg.h"
#include "server.h"
#include "utils.h"

// ============================================================
// service.h — 服务（Service）管理
//
// Service 实现 ROS2 风格的请求/应答（Request/Response）通信模式。
// 一个 Service 可以同时关联多个 Client 和多个 Server（本地服务端）。
//
// 层次关系：
//   ServiceBase         — 非模板基类，持有路由元数据和多态接口
//   ServiceT<Req, Rsp>  — 模板子类，实现本地/远端服务调用逻辑
//   ServiceManager      — 全局单例，统一管理所有 Service 的创建与查找
//
// 路由机制：
//   服务端（Server）周期性广播 ServiceBroadcast，每经过一跳 dist+1。
//   各节点收到广播后在 tsp_metas_ 中记录（传输层、跳数、下一跳）。
//   Client 调用时选择跳数最小的路由，通过对应传输层发送请求。
//
// 服务调用流程：
//   Client::call()
//     → Service::call()
//       · 本地有 Server  → localCall()：直接写请求、等待应答信号量
//       · 无本地 Server  → remoteCall()：通过 Transport 转发请求、等待应答
//
// 线程安全：
//   - lock_req_ (Mutex)：保证同一时刻只有一个 Client 在调用
//   - sem_rsp_  (BinarySemaphore)：Server 写回应答后 give，Client 等待 take
//   - CriticalLock：保护版本号和路由距离的原子读写
// ============================================================

namespace uros {

struct TransportBase;

// TransportMeta: 通过特定传输层到达服务端的路由信息
struct TransportMeta {
  TransportBase *tsp = nullptr; // 对应的传输层
  int dist = INT_MAX;           // 到服务端的跳数（广播 dist 字段，越小越优先）
  int sys_nxt = -1;             // 下一跳节点 ID（从广播的 sys_pre 获取）
  int sys_dst = -1;             // 最终目标节点 ID（服务端所在节点，从广播 sys_src 获取）
};

// ServiceBase: 非模板基类，存储服务的公共属性、路由元数据和纯虚接口
struct ServiceBase {
  ServiceBase(const char *name, type_id_t req_type, type_id_t rsp_type,
              size_t req_size, size_t rsp_size)
      : name_(name), req_type_(req_type), rsp_type_(rsp_type),
        req_size_(req_size), rsp_size_(rsp_size) {
    id_ = calc_entry_hash(name);

    sbc_.seq = -1; // 初始序号为 -1，任何有效广播（seq>=0）均更新
  }

  // 属性访问器
  const auto &id() const { return id_; }
  const char *name() const { return name_; }
  const type_id_t &reqType() const { return req_type_; }
  const type_id_t &rspType() const { return rsp_type_; }
  const size_t &reqSize() const { return req_size_; }
  const size_t &rspSize() const { return rsp_size_; }

  // 将传输层注册到本服务（一个 Service 可关联多个传输层，用于多路由）
  bool registerTransport(TransportBase *tsp);

  // 纯虚接口：由 Transport 调用，以原始指针方式发起服务调用/写回应答/写广播
  virtual bool call(TransportBase *tsp, const MsgBase *req, MsgBase *rsp,
                    int timeout_ms) = 0;
  virtual void writeRsp(TransportBase *from_tsp, const MsgBase *rsp) = 0;
  virtual void writeServiceBroadcast(TransportBase *tsp,
                                     const MsgBase *sbc) = 0;

  virtual ~ServiceBase() = default;

protected:
  int id_;              // 服务的全局唯一 ID（名字哈希）
  const char *name_;    // 服务名称字符串
  type_id_t req_type_;  // 请求消息类型 ID
  type_id_t rsp_type_;  // 应答消息类型 ID
  size_t req_size_;     // 请求消息总大小
  size_t rsp_size_;     // 应答消息总大小

  int req_version_ = -1; // 当前请求版本号（每次写入请求 +1）

  ServiceBroadcast sbc_; // 最新收到/发出的服务广播（含跳数和序号）

  // 各传输层对应的路由元数据（下标对应 Transport::id_）
  etl::array<etl::unique_ptr<TransportMeta>, UROS_MAX_TRANSPORTS> tsp_metas_{};
};

// ServiceT<TReq, TRsp>: 模板化服务，实现完整的调用/路由/广播逻辑
template <typename TReq, typename TRsp> struct ServiceT : public ServiceBase {
  using Req = TReq;
  using Rsp = TRsp;
  using Server = ServerT<TReq, TRsp>;
  using Client = ClientT<TReq, TRsp>;

  using ServiceCallback = std::function<void(const Req &, Rsp &)>;

  ServiceT(const char *name)
      : ServiceBase(name, type_id<TReq>(), type_id<TRsp>(), sizeof(TReq),
                    sizeof(TRsp)) {}

  // 创建并注册一个 Client
  ClientT<Req, Rsp> *addClient();
  // 创建并注册一个 Server（绑定回调）
  ServerT<Req, Rsp> *addServer(const ServiceCallback &cb);

  // 处理服务广播：来自本地 Server 的广播
  void writeServiceBroadcast(Server *srv, const ServiceBroadcast &sbc);
  // 处理服务广播：来自传输层的远端广播
  void writeServiceBroadcast(TransportBase *tsp, const MsgBase *sbc) override;

  // Client 发起调用（由 ClientT::call() 调用）
  bool call(Client *cli, const Req &req, Rsp &rsp, int timeout_ms);
  // Transport 发起远端服务调用（由 TransportBase::serviceWork() 调用）
  bool call(TransportBase *tsp, const MsgBase *req, MsgBase *rsp,
            int timeout_ms) override;

  // 本地 Server 写请求：写入 req_ 缓冲并通知所有 Server
  void writeReq(const Req &req);
  // 等待 Server 写回应答（超时返回 false）
  bool waitRsp(Rsp &rsp, int timeout_ms);
  // Server 读请求（版本比较，有新请求才读取）
  bool readReq(Req &, int &ver);
  // 本地 Server 写回应答
  void writeRsp(Server *srv, const Rsp &rsp);

  // 远端服务调用：通过传输层发送请求并等待应答
  bool remoteCall(TransportBase *tsp, const Req &req, Rsp &rsp, int timeout_ms);
  // 路由查找：在 tsp_metas_ 中选择跳数最小的传输层（排除 from_tsp）
  TransportMeta *findRoute(TransportBase *from_tsp);
  // Transport 写回应答（由 TransportBase::recvResponse() 调用）
  void writeRsp(TransportBase *from_tsp, const MsgBase *rsp) override;

protected:
  bool onServering() const { return srvs_.size() > 0; }
  // 写回应答到 rsp_ 并 give 信号量，唤醒等待中的 waitRsp()
  void writeRsp(const Rsp &rsp);

  // 更新本地广播记录（来自 Server，仅判断序号新旧）
  bool updateServiceBroadcast(const ServiceBroadcast &sbc, int seq);
  // 更新远端广播记录（来自 Transport，判断序号和距离）
  bool updateServiceBroadcast(TransportBase *tsp, const ServiceBroadcast &sbc,
                              int seq);
  // 将广播转发给所有传输层（排除广播来源 tsp）
  void forwardServiceBroadcast(TransportBase *tsp, const ServiceBroadcast &sbc);

  // 本地调用路径：直接写请求、等待应答（不经过传输层）
  bool localCall(const Req &req, Rsp &rsp, int timeout_ms);

protected:
  etl::vector<etl::unique_ptr<Server>, UROS_SERVICE_MAX_SRVS> srvs_; // 本地服务端列表
  etl::vector<etl::unique_ptr<Client>, UROS_SERVICE_MAX_CLIS> clis_; // 客户端列表

  Req req_;                  // 请求缓冲（共享，lock_req_ 保护）
  Rsp rsp_;                  // 应答缓冲（共享，CriticalLock 保护）
  Mutex lock_req_;           // 互斥锁：保证同一时刻只有一个 Client 在调用
  BinarySemaphore sem_rsp_;  // 二值信号量：Server 写回应答后 give，Client 等待 take
};

// ServiceManager: 全局单例，负责所有 Service 的创建和查找
struct ServiceManager {

  template <typename Service> static Service *AddService(const char *name) {
    return Instance().addService<Service>(name);
  }

  template <typename Service> static Service *FindService(const char *name) {
    return Instance().findService<Service>(name);
  }

protected:
  static ServiceManager &Instance() {
    static ServiceManager inst;
    return inst;
  }

  template <typename Service> Service *addService(const char *name);

  template <typename Service> Service *findService(const char *name);

protected:
  ServiceManager() = default;
  ServiceManager(const ServiceManager &other) = delete;
  ServiceManager &operator=(const ServiceManager &other) = delete;

protected:
  // 所有已创建的 Service，以 unique_ptr 持有（静态上限 UROS_MAX_SERVICES）
  etl::vector<etl::unique_ptr<ServiceBase>, UROS_MAX_SERVICES> services_;
};

} // namespace uros
