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
  uint8_t system;      // system id
  uint8_t entry;       // topic or service id
  uint8_t participant; // publisher or client id
  uint8_t type : 4;    // 0: normal msg, 1: req, 2: rsp
  uint8_t seq : 4;     // always inc seq
};

struct MsgBase {
  mutable MsgId __id__; // for internal usage only
};

} // namespace uros
