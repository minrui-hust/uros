#pragma once

#include <cstddef>
#include <cstdint>
#include <tuple>

#include "etl/vector.h"

#include "msg.h"
#include "platform.h"

namespace uros {

struct TopicBase;
struct ServiceBase;
struct Router;

struct TopicMeta {
  TopicBase *topic = nullptr;

  // simple ring buffer with depth 2
  int wr = 0;
  int rd = 0;
  etl::unique_ptr<MsgBase> msgs[2];
};

struct ServiceMeta {
  ServiceBase *service = nullptr;
  int dist = -1;
};

struct RequestInfo {
  int tsp_in;            // which transport this request come from
  uint32_t req_id;       // unique global identifier of an request
  int64_t deadline = -1; // after deadline, request is treat as invalid
};

struct TransportBase {

  bool declareTopic(const char *topic_name);

  bool declareService(const char *service_name);

  TransportBase &setId(int id) {
    id_ = id;
    return *this;
  }

  TransportBase &setRouter(Router *router) {
    router_ = router;
    return *this;
  }

  const auto &id() const { return id_; }

  virtual void init() {}

  // put low level message on transport
  bool put(const MsgBase *msg, int from_tsp, int timeout_ms);

  virtual ~TransportBase() = default;

protected:
  virtual bool putNormal(const MsgBase *msg, int from_tsp, int timeout_ms) = 0;
  virtual bool putRequest(const MsgBase *msg, int from_tsp, int timeout_ms) = 0;
  virtual bool putResponse(const MsgBase *msg, int from_tsp,
                           int timeout_ms) = 0;
  virtual bool putServiceBroadcast(const MsgBase *msg, int from_tsp,
                                   int timeout_ms) = 0;
  virtual bool putServiceDiscovery(const MsgBase *msg, int from_tsp,
                                   int timeout_ms) = 0;

protected:
  int32_t id_ = -1;
  Router *router_ = nullptr;
  uint32_t topic_bit_mask_ = 0;
  etl::array<TopicMeta, UROS_MAX_TOPICS> topic_metas_;       // TODO: init
  etl::array<ServiceMeta, UROS_MAX_SERVICES> service_metas_; // TODO: init
};

struct Router {
  void addTransport(TransportBase *tsp);

  bool route(const MsgBase *msg, int from_tsp, int to_tsp = -1,
             int timeout_ms = 0);

protected:
  bool routeNormal(const MsgBase *msg, int from_tsp, int to_tsp,
                   int timeout_ms);
  bool routeRequest(const MsgBase *msg, int from_tsp, int to_tsp,
                    int timeout_ms);
  bool routeResponse(const MsgBase *msg, int from_tsp, int to_tsp,
                     int timeout_ms);
  bool routeServiceBroadcast(const MsgBase *msg, int from_tsp, int to_tsp,
                             int timeout_ms);
  bool routeServiceDiscovery(const MsgBase *msg, int from_tsp, int to_tsp,
                             int timeout_ms);

  bool broadcast(const MsgBase *msg, int from_tsp, int timeout_ms);

protected:
  etl::vector<TransportBase *, UROS_MAX_TRANSPORT> transports_;
};

struct TransportLocal;

struct TransportManager {
  template <typename Transport> static Transport *AddTransport() {
    return Instance().addTransport<Transport>();
  }

  static TransportLocal *GetTransportLocal() {
    return Instance().getTransportLocal();
  }

  static void Init() { return Instance().init(); }

protected:
  static TransportManager &Instance() {
    static TransportManager inst;
    return inst;
  }

  template <typename Transport> Transport *addTransport();

  TransportLocal *getTransportLocal();

  void init();

protected:
  TransportManager();
  TransportManager(const TransportManager &other) = delete;
  TransportManager &operator=(const TransportManager &other) = delete;

protected:
  Router router_;
  etl::vector<etl::unique_ptr<TransportBase>, UROS_MAX_TRANSPORT> transports_;
};

} // namespace uros
