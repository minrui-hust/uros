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
                       int timeout_ms) {
    return service->emitRequest(req, timeout_ms);
  }
};

} // namespace uros
