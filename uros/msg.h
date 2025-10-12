#pragma once

#include <cstdint>

namespace uros {

enum MsgType {
  MsgTypeNormal = 0,
  MsgTypeRequest = 1,
  MsgTypeResponse = 2,
};

// all message should derived from this
struct __attribute__((packed)) MsgId {
  uint8_t system;            // system id
  uint8_t type : 2;          // 0: normal msg, 1: req, 2: rsp
  uint8_t topic_service : 7; // topic or service id
  uint8_t pub_cli : 7;       // publisher or client id
  uint8_t seq;               // always inc seq
};

struct __attribute__((packed)) MsgBase {
  mutable MsgId __id__; // for internal usage only
};

} // namespace uros
