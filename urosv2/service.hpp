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
  TransportManager::GetTransportLocal()->sendReq(this, req); // TODO: timeout
  return waitRsp(rsp, timeout_ms);
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
void ServiceT<Req, Rsp>::writeRsp(const Rsp &rsp) {
  TransportManager::GetTransportLocal()->sendRsp(this, rsp);
}

template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::callLocal(const Req &req, Rsp &rsp, int timeout_ms) {
  int64_t enter_ms = NowMilli();

  int timeout_now = etl::min(
      timeout_ms, etl::max(timeout_ms - int(NowMilli() - enter_ms), 0));
  LockGuard<Mutex> lg(lock_req_, timeout_now);
  if (!lg.locked()) {
    return false;
  }

  { // update the request
    LockGuard<CriticalLock> lg;
    req_ = req;
    ++req_version_;
  }

  // notify the server
  srvs_[0]->notify();

  // wait for response
  timeout_now = etl::min(timeout_ms,
                         etl::max(timeout_ms - int(NowMilli() - enter_ms), 0));
  auto ret = waitRsp(rsp, timeout_now);

  return ret;
}

template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::callRemote(const Req &req, Rsp &rsp, int timeout_ms) {
  int64_t enter_ms = NowMilli();

  int timeout_now = etl::min(
      timeout_ms, etl::max(timeout_ms - int(NowMilli() - enter_ms), 0));
  LockGuard<Mutex> lg(lock_req_, timeout_now);
  if (!lg.locked()) {
    return false;
  }

  // send request via transport
  // TODO： this may need timeout?
  TransportManager::GetTransportLocal()->sendReq(this, req);

  // wait for response
  timeout_now = etl::min(timeout_ms,
                         etl::max(timeout_ms - int(NowMilli() - enter_ms), 0));
  auto ret = waitRsp(rsp, timeout_now);

  return ret;
}

template <typename Req, typename Rsp>
void ServiceT<Req, Rsp>::recvRsp(const MsgBase *msg) {
  // !!!NOTE!!! transport layer should make sure:
  // 1. msg is a Response
  // 2. msg's entry is current service
  // if these two does not meets, which means transport layer has an bug
  {
    LockGuard<CriticalLock> lg;
    rsp_ = *static_cast<const Rsp *>(msg);
    ++rsp_version_;
  }

  sem_rsp_.give();
}

template <typename Req, typename Rsp>
void ServiceT<Req, Rsp>::recvReq(const MsgBase *msg) {
  if (srvs_.size() <= 0) {
    return;
  }

  LockGuard<Mutex> lg(lock_req_);

  { // update the request
    LockGuard<CriticalLock> lg;
    req_ = *static_cast<const Req *>(msg);
    ++req_version_;
  }

  srvs_[0]->notify();
}

template <typename Req, typename Rsp>
bool ServiceT<Req, Rsp>::waitRsp(Rsp &rsp, int timeout_ms) {
  bool rsp_ok = false;
  int64_t enter_ms = NowMilli();
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
  } while (rsp_ok);

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
