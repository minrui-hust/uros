#pragma once

#include <cstdint>

namespace uros {

enum class MsgType : uint8_t {
  MsgTypeNormal = 0,
  MsgTypeRequest = 1,
  MsgTypeResponse = 2,
  MsgTypeServiceBroadcast = 3,
  MsgTypeServiceDiscovery = 4,
}; // this should be 1 bytes

// all message should derived from this
struct __attribute__((packed)) MsgId {
  uint8_t system : 5;      // system id
  uint8_t entry : 6;       // topic or service id
  uint8_t participant : 5; // publisher or client id on one topic or service
  uint8_t seq; // always inc seq, !!! WARN, do not change size of this !!!
};             // this should be 3 bytes
static_assert(sizeof(MsgId) == 3);

struct __attribute__((packed)) MsgMeta {
  MsgType type;
  MsgId id;
}; // this should be 4 bytes
static_assert(sizeof(MsgMeta) == 4);

struct MsgBase {
  mutable MsgMeta __meta__; // for internal usage only
};

} // namespace uros
