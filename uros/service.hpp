#pragma once

#include "platform.h"

#include "client.h"
#include "server.h"
#include "service.h"
#include "system.h"
#include "transport.h"

namespace uros {

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

template <typename Req, typename Rsp>
ServerT<Req, Rsp> *ServiceT<Req, Rsp>::addServer(const ServiceCallback &cb) {
  if (srvs_.full()) {
    return nullptr;
  }

  auto srv = new ClientT<Req, Rsp>(srvs_.size());
  CHECK(srv);

  srv->bind(this, cb);

  srvs_.emplace_back(srv);

  return srv;
}

template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::call(Client *cli, const Req &req, Rsp &rsp,
                              int timeout_ms) {
  if (onServering()) {
    return localCall(req, rsp, timeout_ms);
  } else {
    return remoteCall(nullptr, req, rsp, timeout_ms);
  }
}

template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::call(TransportBase *tsp, const MsgBase *req,
                              MsgBase *rsp, int timeout_ms) {
  if (onServering()) {
    return localCall(*static_cast<Req *>(req), *static_cast<Rsp *>(rsp),
                     timeout_ms);
  } else {
    return remoteCall(nullptr, *static_cast<Req *>(req),
                      *static_cast<Rsp *>(rsp), timeout_ms);
  }
}

template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::localCall(const Req &req, Rsp &rsp, int timeout_ms) {
  auto enter_ms = now_ms();

  // take lock_req_, caused we only allow one access at same time
  LockGuard<Mutex> lg(lock_req_, timeout_now(timeout_ms, enter_ms));

  writeReq(req);

  // wait for response
  return waitRsp(rsp, timeout_now(timeout_ms, enter_ms));
}

template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::remoteCall(TransportBase *from_tsp, const Req &req,
                                    Rsp &rsp, int timeout_ms) {
  int64_t enter_ms = now_ms();

  // take lock_req_, caused we only allow one access at same time
  LockGuard<Mutex> lg(lock_req_, timeout_now(timeout_ms, enter_ms));

  writeReq(req);

  // find route
  auto meta = findRoute(from_tsp);
  if (!meta) {
    UROS_PRINT("failed to find route for service %d\n", id_);
    return false;
  }

  // send request via transport
  req.__meta__.id.req.sys_from = System::Id();
  req.__meta__.id.req.sys_to = meta->sys_nxt;
  if (!meta->tsp->sendReq(this, req, timeout_now(timeout_ms, enter_ms))) {
    return false;
  }

  // wait for response
  return waitRsp(rsp, timeout_now(timeout_ms, enter_ms));
}

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
    return tsp_metas_[best_idx];
  }

  return nullptr;
}

template <typename Req, typename Rsp>
template <typename Server>
void ServiceT<Req, Rsp>::writeServiceBroadcast(Server *srv, const MsgBase sbc) {
  if (updateServiceBroadcast(nullptr, sbc, sbc.__meta__.id.sbc.seq)) {
    forwardServiceBroadcast(nullptr, sbc);
  }
}

template <typename Req, typename Rsp>
void ServiceT<Req, Rsp>::writeServiceBroadcast(TransportBase *tsp,
                                               const MsgBase *sbc) {
  if (onServering()) {
    return;
  }

  if (updateServiceBroadcast(tsp, *sbc, sbc->__meta__.id.sbc.seq)) {
    forwardServiceBroadcast(tsp, *sbc);
  }
}

template <typename Req, typename Rsp>
void ServiceT<Req, Rsp>::writeReq(const Req &req) {
  {
    LockGuard<CriticalLock> lg;
    req_ = req;
    ++req_version_;
  }
  for (auto &srv : srvs_) {
    srv->notify();
  }
}

template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::waitRsp(Rsp &rsp, int timeout_ms) {
  int64_t enter_ms = now_ms();
  bool rsp_ok = false;
  do {
    if (!sem_rsp_.take(timeout_now(timeout_ms, enter_ms))) {
      return false;
    }

    // if we take the semaphore successfully, but rsp_ok is not true,
    // this means older or wrong rsp was routed to this service, we
    // should discarded the rsp and wait again, until timeout

    { // access to rsp_ should be in critical region
      LockGuard<CriticalLock> lg;
      if (req_.__meta__.id.req == rsp_.__meta__.id.rsp) {
        rsp = rsp_;
        rsp_ok = true;
      }
    }
  } while (!rsp_ok);

  return true;
}

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

template <typename Req, typename Rsp>
void ServiceT<Req, Rsp>::writeRsp(Server *srv, const Rsp &rsp) {
  writeRsp(rsp);
}

template <typename Req, typename Rsp>
void ServiceT<Req, Rsp>::writeRsp(TransportBase *from_tsp, const MsgBase *rsp) {
  writeRsp(*static_cast<Rsp *>(rsp));
}

template <typename Req, typename Rsp>
void ServiceT<Req, Rsp>::writeRsp(const Rsp &rsp) {
  {
    LockGuard<CriticalLock> lg;
    rsp_ = rsp;
  }
  sem_rsp_.give();
}

template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::updateServiceBroadcast(TransportBase *tsp,
                                                const MsgBase &sbc, int seq) {
  if (tsp) {
    auto &meta = tsp_metas_[tsp->id()];
    if (meta) {
      LockGuard<CriticalLock> lg;
      if (int8_t(seq - sbc_.__meta__.id.sbc.seq) > 0) {
        sbc_ = sbc;
        sbc_.__meta__.id.sbc.seq = seq;

        if (sbc_.__meta__.id.sbc.dist < meta->dist) {
          meta->dist = sbc_.__meta__.id.sbc.dist;
          meta->sys_nxt = sbc_.__meta__.id.sbc.sys_from;
        }

        return true;
      } else if (int8_t(seq - sbc_.__meta__.id.sbc.seq) == 0) {
        // TODO:
      }
    }
  } else {
    LockGuard<CriticalLock> lg;
    if (int8_t(seq - sbc_.__meta__.id.sbc.seq) > 0) {
      sbc_ = sbc;
      sbc_.__meta__.id.sbc.seq = seq;
      return true;
    }
  }

  return false;
}

template <typename Req, typename Rsp>
void ServiceT<Req, Rsp>::forwardServiceBroadcast(TransportBase *tsp_from,
                                                 const MsgBase &sbc) {
  sbc.__meta__.id.sbc.sys_from = System::Id();
  for (auto &meta : tsp_metas_) {
    if (meta && meta->tsp != tsp_from) {
      meta->tsp->sendServiceBroadcast(this, sbc);
    }
  }
}

template <typename Service>
Service *ServiceManager::addService(const char *name, int id) {
  if ((size_t)id >= services_.size()) {
    return nullptr;
  }

  auto &service = services_[id];
  if (service) {
    if (service->id() == id && strcmp(service->name(), name) == 0) {
      UROS_PRINT("service '%s' already added with same type\n", name);
      return static_cast<Service *>(service.get());
    } else {
      UROS_PRINT("service '%s' already added with different type\n", name);
      return nullptr;
    }
  }

  service = etl::unique_ptr(new Service(name, id));
  CHECK(service);

  return static_cast<Service *>(service.get());
}

template <typename Service>
Service *ServiceManager::findService(const char *name) {
  for (auto i = 0u; i < services_.size(); ++i) {
    auto &srv = services_[i];
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
