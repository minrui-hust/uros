#pragma once

#include <cstdint>

namespace uros {

enum MsgType {
  MsgTypeNormal = 0,
  MsgTypeRequest = 1,
  MsgTypeResponse = 2,
};

// struct __attribute__((packed)) ReqId {
//   uint8_t system : 5;
//   uint8_t service : 6;
//   uint8_t client : 5;
//   int8_t seq;
// };
// static_assert(sizeof(ReqId) == 3);

struct __attribute__((packed)) ReqId {
  uint8_t system : 5;
  uint8_t service : 6;
  uint8_t client : 5;
  int8_t seq;
};
static_assert(sizeof(ReqId) == 3);

struct __attribute__((packed)) RspId {
  uint8_t system : 5;
  uint8_t service : 6;
  uint8_t client : 5;
  uint8_t dist;
};
static_assert(sizeof(ReqId) == 3);

struct __attribute__((packed)) MsgId {
  uint8_t topic;
  int16_t seq;
};
static_assert(sizeof(ReqId) == 3);

// all message should derived from this
struct __attribute__((packed)) MsgMeta {
  uint8_t type;
  union __attribute__((packed)) {
    MsgId msg;
    ReqId req;
    RspId rsp;
  } id;
};
static_assert(sizeof(MsgMeta) == 4);

struct MsgBase {
  mutable MsgMeta __meta__; // for internal usage only
};

} // namespace uros
