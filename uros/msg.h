#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "uros_config.h"

namespace uros {

enum MsgType {
  MsgTypeNormal = 0,
  MsgTypeRequest = 1,
  MsgTypeResponse = 2,
  MsgTypeServiceBroadcast = 3,
};

struct __attribute__((packed)) MsgId {
  uint8_t topic;
  int16_t len; // msg length without header
};
static_assert(sizeof(MsgId) == 3);

struct __attribute__((packed)) SbcId {
  uint32_t sys_from : 5;
  uint32_t service : 6;
  uint32_t dist : 5;
  int8_t seq;
};
static_assert(sizeof(SbcId) == 3);

struct __attribute__((packed)) ReqId {
  uint32_t sys_from : 5;
  uint32_t sys_to : 5;
  uint32_t service : 6;
  uint32_t client : 4;
  uint32_t seq : 4;

  bool operator==(const ReqId &other) {
    return (service == other.service) && (client == other.client) &&
           (seq == other.seq);
  }
};
static_assert(sizeof(ReqId) == 3);

using RspId = ReqId;

struct __attribute__((packed)) MsgMeta {
  uint32_t type : 3;
  uint32_t sys : 5;
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
  mutable int32_t timeout;

  bool match(const ReqBase &other) {
    return __meta__.sys == other.__meta__.sys &&
           __meta__.id.req == other.__meta__.id.rsp;
  }
};
static_assert(sizeof(ReqBase) == 8);

using RspBase = ReqBase;

struct __attribute__((packed)) ByteStream : MsgBase {
  uint8_t data[UROS_MSG_MAX_SIZE - sizeof(MsgBase)];

  int16_t &len() { return __meta__.id.msg.len; }
  const int16_t &len() const { return __meta__.id.msg.len; }

  // override copy constructor and assignment to handle valid data only

  ByteStream(const ByteStream &other) {
    __meta__ = other.__meta__;
    memcpy(data, other.data,
           std::min(size_t(__meta__.id.msg.len),
                    UROS_MSG_MAX_SIZE - sizeof(MsgBase)));
  }

  void operator=(const ByteStream &other) {
    __meta__ = other.__meta__;
    memcpy(data, other.data,
           std::min(size_t(__meta__.id.msg.len),
                    UROS_MSG_MAX_SIZE - sizeof(MsgBase)));
  }
};
static_assert(sizeof(ByteStream) == UROS_MSG_MAX_SIZE);

} // namespace uros

#define UROS_SBC_DIST_MAX ((1 << 5) - 1)
