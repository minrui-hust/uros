#pragma once

#include "topic.h"
#include "transport.h"

namespace uros {

struct TransportLocal : public TransportBase<TransportLocal> {
  template <typename Topic>
  void writeImpl(Topic *topic, const typename Topic::Msg &msg) {
    topic->update(msg);
  }

  template <typename Service>
  bool sendRequestImpl(Service *service, const typename Service::Req &req,
                       int tsp, int timeout_ms) {
    return service->emitRequest(req, tsp, timeout_ms);
  }

  template <typename Service>
  bool sendResponseImpl(Service *service, const typename Service::Rsp &rsp) {
    return service->emitResponse(rsp);
  }
};

} // namespace uros
