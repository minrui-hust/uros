#pragma once

#include <cstdint>

namespace uros {

enum MsgType {
  MsgTypeNormal = 0,
  MsgTypeRequest = 1,
  MsgTypeResponse = 2,
  MsgTypeServiceBroadcast = 3,
  MsgTypeServiceDiscovery = 4,
}; // this should be 1 bytes

// all message should derived from this
struct __attribute__((packed)) MsgMeta {
  uint32_t system : 5;      // system id
  uint32_t entry : 6;       // topic or service id
  uint32_t participant : 5; // publisher or client id on one topic or service
  uint32_t seq : 8; // always inc seq, !!! WARN, do not change size of this !!!
  uint32_t type : 8;
};
static_assert(sizeof(MsgMeta) == 4);

struct MsgBase {
  mutable MsgMeta __meta__; // for internal usage only
};

} // namespace uros
