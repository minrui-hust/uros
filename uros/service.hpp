#pragma once

#include "platform.h"

#include "service.h"

#include "client.h"
#include "server.h"

namespace uros {

template <typename TransportManager, typename TReq, typename TRsp>
void sendRequest(const TReq &req) {
  // TODO
}

template <typename TransportManager, typename TReq, typename TRsp>
bool waitResponse(TRsp &rsp, int timeout_ms) {
  // TODO
}

template <typename TransportManager, typename TReq, typename TRsp>
bool fetchRequest(TReq &req) {
  // TODO
}

template <typename TransportManager, typename TReq, typename TRsp>
void putResponse(const TRsp &rsp) {
  // TODO
}

template <typename TransportManager, typename TReq, typename TRsp>
bool ServiceT<TransportManager, TReq, TRsp>::emitRequest(const TReq &req,
                                                         int timeout_ms) {
  if (!srv_) {
    return false;
  }

  // TODO
}

template <typename TransportManager, typename TReq, typename TRsp>
void routeRequest(const int tsp_id, const ReqBase *req) {
  // TODO
}

template <typename TransportManager, typename TReq, typename TRsp>
void routeResponse(const int tsp_id, const RspBase *rsp) {
  // TODO
}

template <typename Service>
Service *ServiceManager::addService(const char *name, int id) {
  if ((size_t)id >= services_.size()) {
    return nullptr;
  }

  auto &service = services_[id];
  if (service) {
    if (service->id() == id && strcmp(service->name(), name) == 0) {
      return static_cast<Service *>(service.get());
    } else {
      return nullptr;
    }
  }

  service = std::make_unique<Service>(name, id);
  CHECK(service);

  return static_cast<Service *>(service.get());
}

template <typename Service>
Service *ServiceManager::findService(const char *name) {
  for (auto i = 0u; i < services_.size(); ++i) {
    auto &srv = services_[i];
    if (srv != nullptr && strcmp(srv->name(), name) == 0) {
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
