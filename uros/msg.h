#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "uros_config.h"

// ============================================================
// msg.h — uros 消息结构定义
//
// 本文件定义了 uros 中间件中所有在总线上传输的原始消息结构。
// 每条消息都携带一个 8 字节的元数据头 (MsgMeta)，用于在多节点
// 分布式系统中完成消息类型识别、路由寻址和长度校验。
//
// 消息分四类：
//   MsgTypeNormal          普通发布消息（话题 pub/sub）
//   MsgTypeRequest         服务请求
//   MsgTypeResponse        服务应答
//   MsgTypeServiceBroadcast 服务广播（服务端存在性通告）
// ============================================================

namespace uros {

// 消息类型枚举：标识一条消息属于哪种通信模式
enum MsgType {
  MsgTypeNormal = 0,           // 普通话题消息
  MsgTypeRequest = 1,          // 服务调用请求
  MsgTypeResponse = 2,         // 服务调用应答
  MsgTypeServiceBroadcast = 3, // 服务广播（服务端周期性对外宣告自身存在）
};

// MsgMeta: 8 字节消息元数据头，packed 以保证跨平台对齐
// 所有在传输层传递的消息首部均使用该结构，以便接收端完成路由和分发
struct __attribute__((packed)) MsgMeta {
  uint32_t entry_hash; // topic 或 service 名字的 murmur3 哈希，用于快速查找

  // 以下字段各占 4 bit，共 32 bit，描述路由路径
  uint32_t sys_nxt : 4; // 下一跳系统ID（消息要送往的直接邻居节点）
  uint32_t sys_pre : 4; // 前一跳系统ID（消息从哪个节点转发过来）
  uint32_t sys_src : 4; // 源系统ID（消息的最初发送者）
  uint32_t sys_dst : 4; // 目标系统ID（消息最终要到达的节点）
  uint32_t type : 4;    // 消息类型（对应 MsgType 枚举）
  uint32_t len : 12;    // 消息体的有效载荷长度（不含 MsgBase 头部）
};
static_assert(sizeof(MsgMeta) == 8);

// MsgBase: 所有用户自定义消息必须继承的基类
// __meta__ 由中间件在发布/接收时自动填写，用户不应直接修改
struct MsgBase {
  mutable MsgMeta __meta__; // for internal usage only
};
static_assert(sizeof(MsgBase) == 8);

// ServiceBroadcast: 服务端周期性广播的通告消息
// Server 以固定间隔（默认2s）将此消息向所有传输层发送，
// 让其他节点知道该服务存在并能更新路由距离（dist）
struct ServiceBroadcast : MsgBase {
  mutable int16_t seq;  // 广播序号，用于判断是否为更新的广播（防止旧消息覆盖）
  mutable int16_t dist; // 当前跳数距离，每经过一个 Transport 转发时 +1
};
static_assert(sizeof(ServiceBroadcast) == 12);

// 服务广播的最大距离上限（防止 dist 溢出后回绕）
#define UROS_SBC_DIST_MAX INT16_MAX

// ReqBase: 服务请求/应答的基类，在 MsgBase 之上添加了客户端标识和序号
// 请求与应答通过 (entry_hash, sys_src⇄sys_dst, client, seq) 四元组匹配
struct ReqBase : MsgBase {
  mutable int16_t client;  // 发起请求的 Client 在服务中的索引
  mutable int16_t seq;     // 单调递增序号，用于区分同一客户端的多次调用
  mutable int32_t timeout; // 调用超时时间（ms），由 Client 填入

  // 判断一条应答是否对应当前等待中的请求
  // 成功匹配条件：服务ID、src/dst互换、client ID 和序号均一致
  bool match(const ReqBase &other) {
    return __meta__.entry_hash == other.__meta__.entry_hash &&
           __meta__.sys_src == other.__meta__.sys_dst &&
           __meta__.sys_dst == other.__meta__.sys_src &&
           client == other.client && seq == other.seq;
  }
};
static_assert(sizeof(ReqBase) == 16);

// RspBase 与 ReqBase 结构相同，通过 match() 与请求配对
using RspBase = ReqBase;

// ByteStream: 变长字节流消息，用于传输任意二进制数据
// 实际有效长度通过 MsgMeta::len 字段记录，
// 拷贝时仅复制有效长度范围内的数据以提高效率
struct __attribute__((packed)) ByteStream : MsgBase {
  uint8_t data[UROS_MSG_MAX_SIZE - sizeof(MsgBase)]; // 载荷缓冲区

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
