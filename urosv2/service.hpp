#pragma once

#include "platform.h"

#include "client.h"
#include "server.h"
#include "service.h"
#include "transport_local.h"

namespace uros {

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

  // TODO: broadcast service

  auto srv = new ClientT<Req, Rsp>(srvs_.size());
  CHECK(srv);

  srv->bind(this, cb);

  srvs_.emplace_back(srv);

  return srv;
}

template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::call(const Req &req, Rsp &rsp, int timeout_ms) {
  return TransportManager::GetTransportLocal()->call(this, req, timeout_ms);
}

template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::doCall(const Req &req, Rsp &rsp, int timeout_ms) {
  if (srvs_.size() <= 0) { // no server registered
    return false;
  }

  int64_t enter_ms = NowMilli();

  int timeout_now = etl::min(
      timeout_ms, etl::max(timeout_ms - int(NowMilli() - enter_ms), 0));

  // take lock_req_, caused we only allow one access at same time
  LockGuard<Mutex> lg(lock_req_, timeout_now);

  writeReq(req);

  // wait for response
  timeout_now = etl::min(timeout_ms,
                         etl::max(timeout_ms - int(NowMilli() - enter_ms), 0));

  return waitRsp(rsp, timeout_now);
}

template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::doCall(const MsgBase *req, MsgBase *rsp,
                                int timeout_ms) {
  return doCall(*static_cast<Req *>(req), *static_cast<Rsp *>(rsp), timeout_ms);
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
  int gen;
  { // update the request
    LockGuard<CriticalLock> lg;
    req_ = req;
    gen = ++req_version_;
  }

  if (srvs_[0]) {
    srvs_[0]->notify();
  }
}

template <typename Req, typename Rsp>
int ServiceT<Req, Rsp>::writeReq(const MsgBase *req) {
  return writeReq(*static_cast<Req *>(req));
}

template <typename Req, typename Rsp>
void ServiceT<Req, Rsp>::writeRsp(const Rsp &rsp) {
  { // update the request
    LockGuard<CriticalLock> lg;
    rsp_ = rsp;
    ++rsp_version_;
  }
  sem_rsp_.give();
}

template <typename Req, typename Rsp>
void ServiceT<Req, Rsp>::writeRsp(const MsgBase *rsp) {
  writeRsp(*static_cast<Rsp *>(rsp));
}

template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::waitRsp(Rsp &rsp, int timeout_ms) {
  int64_t enter_ms = NowMilli();
  bool rsp_ok = false;
  do {
    int timeout_now = etl::min(
        timeout_ms, etl::max(timeout_ms - int(NowMilli() - enter_ms), 0));
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
