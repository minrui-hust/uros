#pragma once

#include "service.h"
#include "transport_local.h"

namespace uros {

inline bool TransportLocal::routeInNormal(const MsgBase *msg, int from_tsp,
                                          int timeout_ms) {
  // TODO: check for redundant
  auto topic_id = msg->__meta__.entry;
  if (topic_id >= topic_metas_.size()) {
    return false;
  }

  auto &meta = topic_metas_[topic_id];
  if (!meta) {
    return false;
  }

  meta->topic->doWrite(msg);

  return true;
}

inline bool TransportLocal::routeInRequest(const MsgBase *msg, int from_tsp,
                                           int timeout_ms) {
  auto service_id = msg->__meta__.entry;
  if (service_id >= services_.size()) {
    return false;
  }

  auto service = services_[service_id];
  if (!service || !service->serverPresent()) {
    return false;
  }

  if (!service->doCall(msg, rsp, timeout_ms)) {
    return false;
  }

  return routeOut(rsp, -1, timeout_ms);
}

inline bool TransportLocal::routeInResponse(const MsgBase *msg, int from_tsp,
                                            int timeout_ms) {
  auto service_id = msg->__meta__.entry;
  if (service_id >= services_.size()) {
    return false;
  }

  auto service = services_[service_id];
  if (!service) {
    return false;
  }

  service->writeRsp(msg);

  return true;
}

inline bool TransportLocal::routeInServiceBroadcast(const MsgBase *msg,
                                                    int from_tsp,
                                                    int timeout_ms) {
  return false; // TODO
}

inline bool TransportLocal::routeInServiceDiscovery(const MsgBase *msg,
                                                    int from_tsp,
                                                    int timeout_ms) {
  return false; // TODO
}

template <typename Topic>
int TransportLocal::write(Topic *topic, const typename Topic::Msg &msg) {
  int gen = topic->doWrite(msg);
  msg.__meta__.seq = gen;
  router_->route(&msg, id_, -1, 0); // broadcast
  return gen;
}

template <typename Service>
bool TransportLocal::call(Service *service, const typename Service::Req &req,
                          typename Service::Rsp &rsp, int timeout_ms) {
  if (service->serverPresent()) {
    return service->doCall(req, rsp, timeout_ms);
  } else {
    return remoteCall(service, &req, &rsp, timeout_ms);
  }
}

template <typename Service>
bool TransportLocal::remoteCall(Service *service,
                                const typename Service::Req &req,
                                typename Service::Rsp &rsp, int timeout_ms) {
  auto service_id = service->__meta__.entry;
  if (service_id < 0 || service_id >= services_.size() ||
      service != services_[service_id]) {
    return false;
  }

  int64_t enter_ms = NowMilli();

  int timeout_now = etl::min(
      timeout_ms, etl::max(timeout_ms - int(NowMilli() - enter_ms), 0));
  LockGuard<Mutex> lg(service->lock_req_, timeout_now);

  timeout_now = etl::min(timeout_ms,
                         etl::max(timeout_ms - int(NowMilli() - enter_ms), 0));
  if (!routeOut(req, -1, timeout_now)) {
    return false;
  }

  timeout_now = etl::min(timeout_ms,
                         etl::max(timeout_ms - int(NowMilli() - enter_ms), 0));

  return service->waitRsp(rsp, timeout_ms);
}

} // namespace uros
