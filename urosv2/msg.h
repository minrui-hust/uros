#pragma once

#include <cstdint>

namespace uros {

enum MsgType {
  MsgTypeNormal = 0,
  MsgTypeRequest = 1,
  MsgTypeResponse = 2,
  MsgTypeServiceBroadcast = 3,
  MsgTypeServiceDiscovery = 4,
};

// all message should derived from this
struct __attribute__((packed)) MsgId {
  uint8_t system : 5;  // system id
  uint8_t type : 3;    // see MsgType
  uint8_t entry;       // topic or service id
  uint8_t participant; // publisher or client id on one topic or service
  uint8_t seq; // always inc seq, !!! WARN, do not change size of this !!!
};

struct MsgBase {
  mutable MsgId __id__; // for internal usage only
};

} // namespace uros
