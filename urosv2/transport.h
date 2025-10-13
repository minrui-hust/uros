#pragma once

#include <cstddef>
#include <cstdint>
#include <tuple>

#include "etl/vector.h"

#include "msg.h"
#include "platform.h"

namespace uros {

struct TopicBase;
struct Router;

struct TopicMeta {
  const char *name = "";
  TopicBase *topic = nullptr;
};

struct ServiceMeta {
  int dist = -1;
};

struct RequestInfo {
  int tsp_in;            // which transport this request come from
  uint32_t req_id;       // unique global identifier of an request
  int64_t deadline = -1; // after deadline, request is treat as invalid
};

struct TransportBase {

  bool declareTopic(const char *topic_name);

  TransportBase &setId(int id) {
    id_ = id;
    return *this;
  }

  TransportBase &setRouter(Router *router) {
    router_ = router;
    return *this;
  }

  virtual void init() {}

  // put low level message on transport
  virtual bool put(MsgBase *msg, int from_tsp, int timeout_ms) = 0;

protected:
  int32_t id_ = -1;
  Router *router_ = nullptr;
  etl::array<TopicMeta, UROS_MAX_TOPICS> topic_metas_;       // TODO: init
  etl::array<ServiceMeta, UROS_MAX_SERVICES> service_metas_; // TODO: init
};

struct Router {
  void addTransport(TransportBase *tsp);

  bool route(MsgBase *msg, int from_tsp, int to_tsp = -1, int timeout_ms = 0);

protected:
  bool routeNormal(MsgBase *msg, int from_tsp, int to_tsp, int timeout_ms);
  bool routeRequest(MsgBase *msg, int from_tsp, int to_tsp, int timeout_ms);
  bool routeResponse(MsgBase *msg, int from_tsp, int to_tsp, int timeout_ms);
  bool routeServiceBroadcast(MsgBase *msg, int from_tsp, int to_tsp,
                             int timeout_ms);
  bool routeServiceDiscovery(MsgBase *msg, int from_tsp, int to_tsp,
                             int timeout_ms);

  bool broadcast(MsgBase *msg, int from_tsp, int timeout_ms);

protected:
  etl::vector<TransportBase *, UROS_MAX_TRANSPORT> transports_;
};

template <typename... TTransports> struct TransportManagerT {
  // Get the type of the Idx-th transport
  template <size_t Idx>
  using TransportType = std::tuple_element_t<Idx, std::tuple<TTransports...>>;

  static constexpr size_t size = sizeof...(TTransports);

  static void Init() { return Instance().init(); }

  template <size_t Idx> static auto &Transport() {
    return Instance().template transport<Idx>();
  }

  template <typename TTransport> static auto &Transport() {
    return Instance().template transport<TTransport>();
  }

  static auto &Transports() { return Instance().transports(); }

protected:
  static TransportManagerT &Instance() {
    static TransportManagerT inst;
    return inst;
  }

  template <size_t Idx> auto &transport() { return std::get<Idx>(transports_); }

  template <typename TTransport> auto &transport() {
    return std::get<TTransport>(transports_);
  }

  auto &transports() { return transports_; }

  void init();

protected:
  TransportManagerT();
  TransportManagerT(const TransportManagerT &other) = delete;
  TransportManagerT &operator=(const TransportManagerT &other) = delete;

protected:
  Router router_;
  std::tuple<TTransports...> transports_;
};

} // namespace uros
