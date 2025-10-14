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
  if (!srvs_.empty()) {
    return serveLocal(req, rsp, timeout_ms);
  } else {
    return serveRemote(req, rsp, timeout_ms);
  }
}

template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::serveLocal(const Req &req, Rsp &rsp, int timeout_ms) {
  LockGuard<Mutex> lg(lock_req_, timeout_ms);
  if (!lg.locked()) {
    return false;
  }

  // clear maybe out dated rsp
  sem_rsp_.take(0);

  // prepare req and rsp
  req_ = &req;
  rsp_ = &rsp;

  // notify the server to process
  srvs_[0]->notify();

  // wait for server process done
  if (!sem_rsp_.take(timeout_ms)) {
    return false;
  }

  // rsp_ will be assigned by server

  return true;
}

template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::serveRemote(const Req &req, Rsp &rsp, int timeout_ms) {
  LockGuard<Mutex> lg(lock_req_, timeout_ms);
  if (!lg.locked()) {
    return false;
  }

  // clear maybe out dated rsp
  sem_rsp_.take(0);

  // prepare req and rsp
  req_ = &req;
  rsp_ = &rsp;

  // send request via transport
  TransportManager::GetTransportLocal()->sendReq(this, req);

  // wait for server process done
  if (!sem_rsp_.take(timeout_ms)) {
    return false;
  }

  // rsp_ will be assigned in recvRsp

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
