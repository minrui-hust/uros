#pragma once

#include "etl/vector.h"

#include "platform.h"

#include "client.h"
#include "msg.h"
#include "server.h"
#include "utils.h"

namespace uros {

struct ServiceBase {
  ServiceBase(const char *name, int id, type_id_t req_type, type_id_t rsp_type,
              size_t req_size, size_t rsp_size)
      : id_(id), name_(name), req_type_(req_type), rsp_type_(rsp_type),
        req_size_(req_size), rsp_size_(rsp_size) {}

  const auto &id() const { return id_; }

  const char *name() const { return name_; }

  const type_id_t &reqType() const { return req_type_; }

  const type_id_t &rspType() const { return rsp_type_; }

  const size_t &reqSize() const { return req_size_; }

  const size_t &rspSize() const { return rsp_size_; }

  virtual void recvRsp(const MsgBase *msg) = 0;
  virtual void recvReq(const MsgBase *msg) = 0;

  virtual ~ServiceBase() = default;

protected:
  int id_;
  const char *name_;
  type_id_t req_type_;
  type_id_t rsp_type_;
  size_t req_size_;
  size_t rsp_size_;

  etl::vector<etl::unique_ptr<ServerBase>, UROS_SERVICE_MAX_SRVS> srvs_;
  etl::vector<etl::unique_ptr<ClientBase>, UROS_SERVICE_MAX_CLIS> clis_;
};

template <typename TReq, typename TRsp> struct ServiceT : public ServiceBase {
  using Req = TReq;
  using Rsp = TRsp;

  ServiceT(const char *name, int id)
      : ServiceBase(name, id, type_id<TReq>(), type_id<TRsp>(), sizeof(TReq),
                    sizeof(TRsp)) {}

  ClientT<Req, Rsp> *addClient();

  ServerT<Req, Rsp> *
  addServer(const std::function<void(const Req &, Rsp &)> &cb);

  bool call(const Req &req, Rsp &rsp, int timeout_ms);

  void recvRsp(const MsgBase *msg) override;
  void recvReq(const MsgBase *msg) override;

  const Req *req() { return req_; }
  Rsp *rsp() { return rsp_; }
  void notifyRsp() { sem_rsp_.give(); }

protected:
  bool serveLocal(const Req &req, Rsp &rsp, int timeout_ms);
  bool serveRemote(const Req &req, Rsp &rsp, int timeout_ms);

protected:
  Mutex lock_req_;
  BinarySemaphore sem_rsp_;
  const Req *req_ = nullptr;
  Rsp *rsp_ = nullptr;
};

struct ServiceManager {

  template <typename Service>
  static Service *AddService(const char *name, int id) {
    return Instance().addService<Service>(name, id);
  }

  template <typename Service> static Service *FindService(const char *name) {
    return Instance().findService<Service>(name);
  }

protected:
  static ServiceManager &Instance() {
    static ServiceManager inst;
    return inst;
  }

  template <typename Service> Service *addService(const char *name, int id);

  template <typename Service> Service *findService(const char *name);

protected:
  ServiceManager() = default;
  ServiceManager(const ServiceManager &other) = delete;
  ServiceManager &operator=(const ServiceManager &other) = delete;

protected:
  etl::array<etl::unique_ptr<ServiceBase>, UROS_MAX_SERVICES> services_{};
};

} // namespace uros
