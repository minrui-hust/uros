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

namespace uros {

struct TransportBase;

struct TransportMeta {
  TransportBase *tsp = nullptr;
  int dist = INT_MAX;
  int sys_nxt = -1;
  int sys_dst = -1;
};

struct ServiceBase {
  ServiceBase(const char *name, type_id_t req_type, type_id_t rsp_type,
              size_t req_size, size_t rsp_size)
      : name_(name), req_type_(req_type), rsp_type_(rsp_type),
        req_size_(req_size), rsp_size_(rsp_size) {
    id_ = calc_entry_hash(name);

    sbc_.seq = -1;
  }

  // properti accessors
  const auto &id() const { return id_; }
  const char *name() const { return name_; }
  const type_id_t &reqType() const { return req_type_; }
  const type_id_t &rspType() const { return rsp_type_; }
  const size_t &reqSize() const { return req_size_; }
  const size_t &rspSize() const { return rsp_size_; }

  bool registerTransport(TransportBase *tsp);

  // call service from transport
  virtual bool call(TransportBase *tsp, const MsgBase *req, MsgBase *rsp,
                    int timeout_ms) = 0;

  // write response from transport
  virtual void writeRsp(TransportBase *from_tsp, const MsgBase *rsp) = 0;

  // write service broad cast from transport
  virtual void writeServiceBroadcast(TransportBase *tsp,
                                     const MsgBase *sbc) = 0;

  virtual ~ServiceBase() = default;

protected:
  int id_;
  const char *name_;
  type_id_t req_type_;
  type_id_t rsp_type_;
  size_t req_size_;
  size_t rsp_size_;

  int req_version_ = -1;

  ServiceBroadcast sbc_;

  etl::array<etl::unique_ptr<TransportMeta>, UROS_MAX_TRANSPORTS> tsp_metas_{};
};

template <typename TReq, typename TRsp> struct ServiceT : public ServiceBase {
  using Req = TReq;
  using Rsp = TRsp;
  using Server = ServerT<TReq, TRsp>;
  using Client = ClientT<TReq, TRsp>;

  using ServiceCallback = std::function<void(const Req &, Rsp &)>;

  ServiceT(const char *name)
      : ServiceBase(name, type_id<TReq>(), type_id<TRsp>(), sizeof(TReq),
                    sizeof(TRsp)) {}

  ClientT<Req, Rsp> *addClient();

  ServerT<Req, Rsp> *addServer(const ServiceCallback &cb);

  void writeServiceBroadcast(Server *srv, const ServiceBroadcast &sbc);
  void writeServiceBroadcast(TransportBase *tsp, const MsgBase *sbc) override;

  // call the service
  bool call(Client *cli, const Req &req, Rsp &rsp, int timeout_ms);
  bool call(TransportBase *tsp, const MsgBase *req, MsgBase *rsp,
            int timeout_ms) override;

  // interface with server
  bool localCall(const Req &req, Rsp &rsp, int timeout_ms);
  void writeReq(const Req &req);
  bool waitRsp(Rsp &rsp, int timeout_ms);
  bool readReq(Req &, int &ver);
  void writeRsp(Server *srv, const Rsp &rsp);

  bool remoteCall(TransportBase *tsp, const Req &req, Rsp &rsp, int timeout_ms);
  TransportMeta *findRoute(TransportBase *from_tsp);
  void writeRsp(TransportBase *from_tsp, const MsgBase *rsp) override;

protected:
  bool onServering() const { return srvs_.size() > 0; }
  void writeRsp(const Rsp &rsp);

  bool updateServiceBroadcast(const ServiceBroadcast &sbc, int seq);
  bool updateServiceBroadcast(TransportBase *tsp, const ServiceBroadcast &sbc,
                              int seq);
  void forwardServiceBroadcast(TransportBase *tsp, const ServiceBroadcast &sbc);

protected:
  etl::vector<etl::unique_ptr<Server>, UROS_SERVICE_MAX_SRVS> srvs_;
  etl::vector<etl::unique_ptr<Client>, UROS_SERVICE_MAX_CLIS> clis_;

  Req req_;
  Rsp rsp_;
  Mutex lock_req_;
  BinarySemaphore sem_rsp_;
};

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
  etl::vector<etl::unique_ptr<ServiceBase>, UROS_MAX_SERVICES> services_;
};

} // namespace uros
