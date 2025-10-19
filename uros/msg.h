#pragma once

#include <cstdint>

namespace uros {

enum MsgType {
  MsgTypeNormal = 0,
  MsgTypeRequest = 1,
  MsgTypeResponse = 2,
  MsgTypeServiceBroadcast = 3,
};

struct __attribute__((packed)) MsgId {
  uint8_t topic;
  int16_t seq;
};
static_assert(sizeof(MsgId) == 3);

struct __attribute__((packed)) SbcId {
  uint8_t sys_from : 5;
  uint8_t service : 6;
  uint8_t dist : 5;
  int8_t seq;
};
static_assert(sizeof(SbcId) == 3);

struct __attribute__((packed)) ReqId {
  uint8_t sys_from : 5;
  uint8_t sys_to : 5;
  uint8_t service : 6;
  uint8_t client : 4;
  uint8_t seq : 4;
};
static_assert(sizeof(ReqId) == 3);

using RspId = ReqId;

struct __attribute__((packed)) MsgMeta {
  uint8_t type : 3;
  uint8_t sys : 5;
  union __attribute__((packed)) {
    MsgId msg;
    SbcId sbc;
    ReqId req;
    RspId rsp;
  } id;
};
static_assert(sizeof(MsgMeta) == 4);

struct MsgBase {
  mutable MsgMeta __meta__; // for internal usage only
};
static_assert(sizeof(MsgBase) == 4);

struct ReqBase : MsgBase {
  int32_t timeout;
};
static_assert(sizeof(ReqBase) == 8);

using RspBase = ReqBase;

} // namespace uros

#define UROS_SBC_DIST_MAX ((1 << 5) - 1)
