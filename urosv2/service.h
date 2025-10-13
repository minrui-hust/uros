#pragma once

#include "etl/vector.h"

#include "platform.h"

#include "client.h"
#include "msg.h"
#include "server.h"
#include "transport.h"
#include "utils.h"

namespace uros {

struct ServiceBase {
  ServiceBase(const char *name, int id, type_id_t req_type, type_id_t rsp_type,
              size_t req_size, size_t rsp_size)
      : id_(id), name_(name), req_type_(req_type), rsp_type_(rsp_type),
        req_size_(req_size), rsp_size_(rsp_size) {}

  const int32_t &id() const { return id_; }

  const char *name() const { return name_; }

  const type_id_t &reqType() const { return req_type_; }

  const type_id_t &rspType() const { return rsp_type_; }

  const size_t &reqSize() const { return req_size_; }

  const size_t &rspSize() const { return rsp_size_; }

  bool registerServer(ServerBase *srv) {
    srv_ = srv;
    return true; // TODO: borad cast server registration
  }

  bool registerClient(ClientBase *cli) {
    if (clis_.full()) {
      return false;
    }
    clis_.emplace_back(cli);
    return true;
  }

protected:
  int id_;
  const char *name_;
  type_id_t req_type_;
  type_id_t rsp_type_;
  size_t req_size_;
  size_t rsp_size_;

  ServerBase *srv_ = nullptr;
  etl::vector<ClientBase *, UROS_SERVICE_MAX_CLIS> clis_;
};

template <typename TReq, typename TRsp> struct ServiceT : public ServiceBase {
  using Req = TReq;
  using Rsp = TRsp;

  ServiceT(const char *name, int id)
      : ServiceBase(name, id, type_id<TReq>(), type_id<TRsp>(), sizeof(TReq),
                    sizeof(TRsp)) {}

protected:
  TReq req_;
  TRsp rsp_;
};

struct ServiceManager {
  static ServiceManager &Instance() {
    static ServiceManager inst;
    return inst;
  }

  template <typename Service>
  static Service *AddService(const char *name, int id) {
    return Instance().addService<Service>(name, id);
  }

  template <typename Service> static Service *FindService(const char *name) {
    return Instance().findService<Service>(name);
  }

protected:
  template <typename Service> Service *addService(const char *name, int id);

  template <typename Service> Service *findService(const char *name);

protected:
  ServiceManager() = default;
  ServiceManager(const ServiceManager &other) = delete;
  ServiceManager &operator=(const ServiceManager &other) = delete;

protected:
  etl::array<std::unique_ptr<ServiceBase>, UROS_MAX_SERVICES> services_{};
};

} // namespace uros
