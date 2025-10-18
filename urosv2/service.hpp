#pragma once

#include "platform.h"

#include "client.h"
#include "server.h"
#include "service.h"
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
ServerT<Req, Rsp> *ServiceT<Req, Rsp>::addServer(
    const std::function<void(const Req &, Rsp &)> &cb) {
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
bool ServiceT<Req, Rsp>::call(const Req &req, Rsp &rsp, TransportBase *tsp,
                              int timeout_ms) {

  // TODO: reject redundant call(multi path)

  if (srvs_.size() > 0) {
    return callLocal(req, rsp, tsp, timeout_ms);
  } else {
    return callRemote(req, rsp, tsp, timeout_ms);
  }
}

template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::call(const MsgBase *req, MsgBase *rsp,
                              TransportBase *tsp, int timeout_ms) {
  return call(*static_cast<Req *>(req), *static_cast<Rsp *>(rsp), tsp,
              timeout_ms);
}

template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::callLocal(const Req &req, Rsp &rsp, TransportBase *tsp,
                                   int timeout_ms) {
  int64_t enter_ms = now_ms();

  int timeout_now =
      etl::min(timeout_ms, etl::max(timeout_ms - int(now_ms() - enter_ms), 0));

  // take lock_req_, caused we only allow one access at same time
  LockGuard<Mutex> lg(lock_req_, timeout_now);

  writeReq(req);

  // wait for response
  timeout_now =
      etl::min(timeout_ms, etl::max(timeout_ms - int(now_ms() - enter_ms), 0));

  return waitRsp(rsp, timeout_now);
}

template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::callRemote(const Req &req, Rsp &rsp,
                                    TransportBase *from_tsp, int timeout_ms) {
  int64_t enter_ms = now_ms();

  // lock request
  int timeout_now =
      etl::min(timeout_ms, etl::max(timeout_ms - int(now_ms() - enter_ms), 0));
  LockGuard<Mutex> lg(lock_req_, timeout_now);

  // find route
  auto to_tsp = findRoute(from_tsp);
  if (!to_tsp) {
    UROS_PRINT("failed to find route for service %d\n", id_);
    return false;
  }

  // send request
  timeout_now =
      etl::min(timeout_ms, etl::max(timeout_ms - int(now_ms() - enter_ms), 0));
  if (!to_tsp->sendReq(this, req, timeout_ms)) {
    return false;
  }

  // wait for response
  timeout_now =
      etl::min(timeout_ms, etl::max(timeout_ms - int(now_ms() - enter_ms), 0));

  return waitRsp(rsp, timeout_now);
}

template <typename Req, typename Rsp>
TransportBase *ServiceT<Req, Rsp>::findRoute(TransportBase *from_tsp) {
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

  // if found
  if (min_dist < INT_MAX) {
    return tsp_metas_[best_idx]->tsp;
  }

  // not found
  return nullptr;
}

template <typename Req, typename Rsp>
void ServiceT<Req, Rsp>::writeServiceBroadcast(TransportBase *tsp,
                                               const ServiceBroadcast &sbc) {
}

template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::readReq(Req &req, int &ver) {
  LockGuard<CriticalLock> lg;
  if (req_version_ <= ver) {
    return false;
  }
  req = req_;
  ver = req_version_;
  return true;
}

template <typename Req, typename Rsp>
int ServiceT<Req, Rsp>::writeReq(const Req &req) {
  // TODO: redundant request
  int gen;
  {
    LockGuard<CriticalLock> lg;
    req_ = req;
    gen = ++req_version_;
  }

  CHECK(srvs_.size() > 0 && srvs_[0]);
  srvs_[0]->notify();
}

template <typename Req, typename Rsp>
void ServiceT<Req, Rsp>::writeRsp(const Rsp &rsp, TransportBase *from_tsp) {
  auto &meta = tsp_metas_[from_tsp->id()];
  {
    LockGuard<CriticalLock> lg;
    rsp_ = rsp;
    if (meta) {
      auto &rsp_dist = rsp_.__meta__.id.rsp.dist;
      if (rsp_dist < etl::integral_limits<decltype(rsp_dist)>()) {
        ++rsp_dist;
      }
      if (rsp_dist < meta->dist) {
        meta->dist = rsp_dist;
      }
    }
  }
  sem_rsp_.give();
}

template <typename Req, typename Rsp>
void ServiceT<Req, Rsp>::writeRsp(const MsgBase *rsp, TransportBase *from_tsp) {
  writeRsp(*static_cast<Rsp *>(rsp), from_tsp);
}

template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::waitRsp(Rsp &rsp, int timeout_ms) {
  int64_t enter_ms = now_ms();
  bool rsp_ok = false;
  do {
    int timeout_now = etl::min(
        timeout_ms, etl::max(timeout_ms - int(now_ms() - enter_ms), 0));
    if (!sem_rsp_.take(timeout_now)) {
      return false;
    }

    // if we take the semaphore successfully, but rsp_ok is not true,
    // this means older or wrong rsp was routed to this service, we
    // should discarded the rsp and wait again, until timeout

    { // access to rsp_ should be in critical region
      LockGuard<CriticalLock> lg;
      if (req_.__meta__.id == rsp_.__meta__.id) {
        rsp = rsp_;
        rsp_ok = true;
      }
    }
  } while (!rsp_ok);

  return true;
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
