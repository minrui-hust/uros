#pragma once

#include "topic.h"
#include "transport.h"

namespace uros {

struct TransportLocal : public TransportBase<TransportLocal> {
  template <typename Topic>
  void writeImpl(Topic *topic, const typename Topic::Msg &msg) {
    topic->update(msg);
  }
};

} // namespace uros
