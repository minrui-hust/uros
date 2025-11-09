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
  uint32_t entry_hash; // topic/service hash id
  uint32_t sys_nxt : 4;
  uint32_t sys_pre : 4;
  uint32_t sys_src : 4;
  uint32_t sys_dst : 4;
  uint32_t type : 4;
  uint32_t len : 12;
};
static_assert(sizeof(MsgMeta) == 8);

struct MsgBase {
  mutable MsgMeta __meta__; // for internal usage only
};
static_assert(sizeof(MsgBase) == 8);

struct ServiceBroadcast : MsgBase {
  mutable int16_t seq;
  mutable int16_t dist;
};
static_assert(sizeof(ServiceBroadcast) == 12);

#define UROS_SBC_DIST_MAX INT16_MAX

struct ReqBase : MsgBase {
  mutable int16_t client;
  mutable int16_t seq;
  mutable int32_t timeout;

  bool match(const ReqBase &other) {
    return __meta__.entry_hash == other.__meta__.entry_hash &&
           __meta__.sys_src    == other.__meta__.sys_dst &&
           __meta__.sys_dst    == other.__meta__.sys_src &&
           client == other.client &&
           seq == other.seq;
  }
};
static_assert(sizeof(ReqBase) == 16);

using RspBase = ReqBase;

struct __attribute__((packed)) ByteStream : MsgBase {
  uint8_t data[UROS_MSG_MAX_SIZE - sizeof(MsgBase)];

  void setLen(size_t len) { __meta__.len = len; }
  size_t getLen() const { return __meta__.len; }

  ByteStream() = default;

  // override copy constructor and assignment to handle valid data only
  ByteStream(const ByteStream &other) {
    __meta__ = other.__meta__;
    memcpy(data, other.data,
           std::min(size_t(__meta__.len), UROS_MSG_MAX_SIZE - sizeof(MsgBase)));
  }

  void operator=(const ByteStream &other) {
    __meta__ = other.__meta__;
    memcpy(data, other.data,
           std::min(size_t(__meta__.len), UROS_MSG_MAX_SIZE - sizeof(MsgBase)));
  }
};
static_assert(sizeof(ByteStream) == UROS_MSG_MAX_SIZE);

} // namespace uros
