#pragma once

#include "platform.h"

#include "client.h"
#include "server.h"
#include "service.h"
#include "system.h"
#include "transport.h"

// ============================================================
// service.hpp — ServiceBase / ServiceT / ServiceManager 实现
// ============================================================

namespace uros {

// registerTransport(): 将传输层 tsp 注册到本服务的 tsp_metas_ 数组。
// tsp_metas_ 以 Transport ID 为下标（稀疏数组）。
// 注册后，收到来自该传输层的服务广播时，可更新对应的路由距离。
inline bool ServiceBase::registerTransport(TransportBase *tsp) {
  auto tsp_id = tsp->id();
  if (tsp_id >= tsp_metas_.size()) {
    return false;
  }

  auto &meta = tsp_metas_[tsp_id];
  if (meta) {
    UROS_PRINT("tsp '%d' already register on service %d \n", tsp_id, id_);
    return false;
  }

  meta.reset(new TransportMeta);
  meta->tsp = tsp;

  UROS_PRINT("register transport '%d' to service %d succeed\n", tsp_id, id_);

  return true;
}

// addClient(): 在服务上创建并注册一个 Client。
// Client 由 Service 持有（unique_ptr），Node 只保存裸指针。
template <typename Req, typename Rsp>
ClientT<Req, Rsp> *ServiceT<Req, Rsp>::addClient() {
  if (clis_.full()) {
    return nullptr;
  }

  auto cli = new ClientT<Req, Rsp>(clis_.size());
  CHECK(cli);

  cli->connect(this);

  clis_.emplace_back(cli);

  return cli;
}

// addServer(): 在服务上创建并注册一个 Server（绑定回调）。
// Server 由 Service 持有（unique_ptr），Node 只保存裸指针。
template <typename Req, typename Rsp>
ServerT<Req, Rsp> *ServiceT<Req, Rsp>::addServer(const ServiceCallback &cb) {
  if (srvs_.full()) {
    return nullptr;
  }

  auto srv = new ServerT<Req, Rsp>(srvs_.size());
  CHECK(srv);

  srv->bind(this, cb);

  srvs_.emplace_back(srv);

  return srv;
}

// call(cli, req, rsp): 由 Client 发起调用的主入口。
// 判断本节点是否有本地 Server：
//   - 有 → localCall()：直接写请求缓冲、等待信号量，无网络开销
//   - 无 → remoteCall()：通过路由找到最近的传输层，发送请求，等待应答
template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::call(Client *cli, const Req &req, Rsp &rsp,
                              int timeout_ms) {
  UROS_PRINT("call service '0x%x' from client '%d'\n", id_, cli->id());
  if (onServering()) {
    return localCall(req, rsp, timeout_ms);
  } else {
    return remoteCall(nullptr, req, rsp, timeout_ms);
  }
}

// call(tsp, req, rsp): 由 Transport 在 serviceWork() 中调用，
// 处理来自远端节点的服务请求，路径选择同上。
template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::call(TransportBase *tsp, const MsgBase *req,
                              MsgBase *rsp, int timeout_ms) {
  if (onServering()) {
    return localCall(*static_cast<const Req *>(req), *static_cast<Rsp *>(rsp),
                     timeout_ms);
  } else {
    return remoteCall(nullptr, *static_cast<const Req *>(req),
                      *static_cast<Rsp *>(rsp), timeout_ms);
  }
}

// localCall(): 本地服务调用路径。
// 步骤：
//   1. 获取 lock_req_（互斥锁），保证同一时刻只有一个调用在处理
//   2. 填写 sys_dst（本节点）并写入请求缓冲，通知 Server 的 spinOnce()
//   3. 等待 Server 调用 writeRsp() 后 give 的 sem_rsp_ 信号量
template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::localCall(const Req &req, Rsp &rsp, int timeout_ms) {
  UROS_PRINT("localCall on service '0x%x'\n", id_);

  auto enter_ms = now_ms();

  // take lock_req_, caused we only allow one access at same time
  LockGuard<Mutex> lg(lock_req_, timeout_now(timeout_ms, enter_ms));
  if (!lg.locked()) {
    UROS_PRINT("localCall failed to get req lock\n");
    return false;
  }

  req.__meta__.sys_dst = System::Id();

  writeReq(req);

  // wait for response
  return waitRsp(rsp, timeout_now(timeout_ms, enter_ms));
}

// remoteCall(): 远端服务调用路径。
// 步骤：
//   1. 获取 lock_req_
//   2. findRoute() 找最近路由（跳数最小的 tsp_meta）
//   3. 填写 sys_dst / sys_nxt 并写入请求缓冲
//   4. 通过对应传输层发送请求
//   5. 等待 Transport 收到应答后调用 writeRsp() 触发的信号量
template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::remoteCall(TransportBase *from_tsp, const Req &req,
                                    Rsp &rsp, int timeout_ms) {
  int64_t enter_ms = now_ms();

  // take lock_req_, caused we only allow one access at same time
  LockGuard<Mutex> lg(lock_req_, timeout_now(timeout_ms, enter_ms));

  // find route
  auto meta = findRoute(from_tsp);
  if (!meta) {
    UROS_PRINT("failed to find route for service 0x%x\n", id_);
    return false;
  }

  req.__meta__.sys_dst = meta->sys_dst;
  req.__meta__.sys_nxt = meta->sys_nxt;

  writeReq(req);

  // send request via transport
  if (!meta->tsp->sendReq(this, req, timeout_now(timeout_ms, enter_ms))) {
    return false;
  }

  // wait for response
  return waitRsp(rsp, timeout_now(timeout_ms, enter_ms));
}

// findRoute(): 在 tsp_metas_ 中选择路由距离（dist）最小的传输层。
// 排除 from_tsp（避免请求回路）。dist == INT_MAX 表示该路由不可达。
template <typename Req, typename Rsp>
TransportMeta *ServiceT<Req, Rsp>::findRoute(TransportBase *from_tsp) {
  // find the transport to send request
  int best_idx = -1;
  int min_dist = INT_MAX;
  {
    LockGuard<CriticalLock> lg; // protect dist
    for (auto i = 0; i < tsp_metas_.size(); ++i) {
      if (tsp_metas_[i] && tsp_metas_[i]->dist < min_dist &&
          tsp_metas_[i]->tsp != from_tsp) {
        best_idx = i;
        min_dist = tsp_metas_[i]->dist;
      }
    }
  }

  // found
  if (min_dist < INT_MAX) {
    return tsp_metas_[best_idx].get();
  }

  return nullptr;
}

// writeServiceBroadcast(Server): 来自本地 Server 的广播。
// 更新本地广播记录（仅序号判断），然后向所有传输层转发。
template <typename Req, typename Rsp>
void ServiceT<Req, Rsp>::writeServiceBroadcast(Server *srv,
                                               const ServiceBroadcast &sbc) {
  if (updateServiceBroadcast(sbc, sbc.seq)) {
    forwardServiceBroadcast(nullptr, sbc);
  } else {
    UROS_PRINT("local updateServiceBroadcast failed: seq(%d)\n", sbc.seq);
  }
}

// writeServiceBroadcast(tsp): 来自传输层的远端广播。
// 如果本节点有本地 Server，忽略该广播（多 Server 场景可能引发问题，已记录日志）。
// 否则更新路由记录（序号+距离双条件），然后转发给其他传输层。
template <typename Req, typename Rsp>
void ServiceT<Req, Rsp>::writeServiceBroadcast(TransportBase *tsp,
                                               const MsgBase *msg) {
  if (onServering()) {
    UROS_PRINT("writeServiceBroadcast called from transport while service in "
               "on serving, this may caused by multiple server\n");
    return;
  }

  auto sbc = static_cast<const ServiceBroadcast *>(msg);

  if (updateServiceBroadcast(tsp, *sbc, sbc->seq)) {
    forwardServiceBroadcast(tsp, *sbc);
  } else {
    UROS_PRINT("remote updateServiceBroadcast failed: seq(%d)\n", sbc->seq);
  }
}

// writeReq(): 在临界区内更新请求缓冲并递增版本号，
// 然后通知所有本地 Server（设置其事件位，触发 Node::spinOnce()）。
template <typename Req, typename Rsp>
void ServiceT<Req, Rsp>::writeReq(const Req &req) {
  UROS_PRINT("service '0x%x' writeReq\n", id_);
  {
    LockGuard<CriticalLock> lg;
    req_ = req;
    ++req_version_;
  }
  for (auto &srv : srvs_) {
    srv->notify();
  }
}

// waitRsp(): 等待 Server 写回应答（超时返回 false）。
// 使用循环 + 信号量：Server 每次 writeRsp() 后 give 信号量，
// Client 取到信号量后验证 req/rsp 匹配（防止旧应答误匹配），
// 匹配失败则继续等待，直到超时。
template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::waitRsp(Rsp &rsp, int timeout_ms) {
  int64_t enter_ms = now_ms();
  bool rsp_ok = false;
  do {
    if (!sem_rsp_.take(timeout_now(timeout_ms, enter_ms))) {
      UROS_PRINT("waitRsp timeout\n");
      return false;
    }

    // if we take the semaphore successfully, but rsp_ok is not true,
    // this means older or wrong rsp was routed to this service, we
    // should discarded the rsp and wait again, until timeout

    { // access to rsp_ should be in critical region
      LockGuard<CriticalLock> lg;
      if (req_.match(rsp_)) {
        rsp = rsp_;
        rsp_ok = true;
      } else {
        UROS_PRINT("waitRsp req&rsp mismatch:\n");
        UROS_PRINT("req: sys_src(%d), sys_dst(%d), service(0x%x), client(%d), "
                   "seq(%d)\n",
                   req_.__meta__.sys_src, req_.__meta__.sys_dst,
                   req_.__meta__.entry_hash, req_.client, req_.seq);
        UROS_PRINT("rsp: sys_src(%d), sys_dst(%d), service(0x%x), client(%d), "
                   "seq(%d)\n",
                   rsp_.__meta__.sys_src, rsp_.__meta__.sys_dst,
                   rsp_.__meta__.entry_hash, rsp_.client, rsp_.seq);
      }
    }
  } while (!rsp_ok);

  return true;
}

// readReq(): 由 Server::spinOnce() 调用，以版本比较方式读取最新请求。
// 若请求版本 > 本地版本，则读取并更新版本，返回 true。
template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::readReq(Req &req, int &ver) {
  LockGuard<CriticalLock> lg;
  if (int(req_version_ - ver) > 0) {
    req = req_;
    ver = req_version_;
    return true;
  }
  return false;
}

// writeRsp(Server): 来自本地 Server 的应答，委托给 writeRsp(Rsp)
template <typename Req, typename Rsp>
void ServiceT<Req, Rsp>::writeRsp(Server *srv, const Rsp &rsp) {
  writeRsp(rsp);
}

// writeRsp(tsp): 来自传输层的远端应答（由 TransportBase::recvResponse() 调用）
template <typename Req, typename Rsp>
void ServiceT<Req, Rsp>::writeRsp(TransportBase *from_tsp, const MsgBase *rsp) {
  writeRsp(*static_cast<const Rsp *>(rsp));
}

// writeRsp(rsp): 在临界区内更新应答缓冲，然后 give 信号量唤醒 waitRsp()
template <typename Req, typename Rsp>
void ServiceT<Req, Rsp>::writeRsp(const Rsp &rsp) {
  {
    LockGuard<CriticalLock> lg;
    rsp_ = rsp;
  }
  sem_rsp_.give();
}

// updateServiceBroadcast(sbc, seq): 更新本地广播记录（来自本地 Server）。
// 仅在序号更新时（seq > sbc_.seq）才更新，使用有符号 int16_t 差值处理回绕。
template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::updateServiceBroadcast(const ServiceBroadcast &sbc,
                                                int seq) {
  LockGuard<CriticalLock> lg;
  if (int16_t(seq - sbc_.seq) > 0) {
    sbc_ = sbc;
    sbc_.seq = seq;
    return true;
  }
  return false;
}

// updateServiceBroadcast(tsp, sbc, seq): 更新远端广播路由记录。
// 更新条件（二选一）：
//   1. 序号更新（seq > sbc_.seq）
//   2. 序号相同但距离更短（better_dist）
// 同时更新 tsp_meta 中的路由信息（dist, sys_nxt, sys_dst）。
template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::updateServiceBroadcast(TransportBase *tsp,
                                                const ServiceBroadcast &sbc,
                                                int seq) {
  auto &meta = tsp_metas_[tsp->id()];
  CHECK(meta);

  LockGuard<CriticalLock> lg;
  bool better_dist = sbc.dist < meta->dist;
  int8_t seq_diff = seq - sbc_.seq;

  if (seq_diff > 0 || (seq_diff == 0 && better_dist)) {
    sbc_ = sbc;
    sbc_.seq = seq;
    if (better_dist) {
      meta->dist = sbc.dist;
      meta->sys_nxt = sbc.__meta__.sys_pre; // 下一跳 = 广播的前一跳
      meta->sys_dst = sbc.__meta__.sys_src; // 目标 = 广播的原始来源
    }
    return true;
  }

  return false;
}

// forwardServiceBroadcast(): 将服务广播转发给所有已注册的传输层（排除来源 tsp）。
// 转发前设置 sys_pre = 本节点 ID，以便下一跳能正确记录路径。
template <typename Req, typename Rsp>
void ServiceT<Req, Rsp>::forwardServiceBroadcast(TransportBase *tsp_from,
                                                 const ServiceBroadcast &sbc) {
  sbc.__meta__.sys_pre = System::Id();
  for (auto &meta : tsp_metas_) {
    if (meta && meta->tsp != tsp_from) {
      meta->tsp->sendServiceAnnounce(this, sbc, -1);
    }
  }
}

// addService(): 创建新服务并加入全局列表
template <typename Service>
Service *ServiceManager::addService(const char *name) {
  if (services_.full()) {
    return nullptr;
  }

  auto service = new Service(name);
  CHECK(service);
  services_.emplace_back(service);

  return service;
}

// findService(): 按名称查找服务，并验证请求/应答类型匹配
template <typename Service>
Service *ServiceManager::findService(const char *name) {
  for (auto &srv : services_) {
    if (srv.get() != nullptr && strcmp(srv->name(), name) == 0) {
      if constexpr (std::is_same_v<Service, ServiceBase>) {
        return srv.get();
      } else {
        if (srv->reqType() == type_id<typename Service::Req>() &&
            srv->rspType() == type_id<typename Service::Rsp>()) {
          return static_cast<Service *>(srv.get());
        } else {
          UROS_PRINT("service found but msg type mismatch\n");
          return nullptr; // topic exist but type mismatch
        }
      }
    }
  }

  return nullptr;
}

} // namespace uros
