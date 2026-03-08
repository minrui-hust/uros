#pragma once

#include <cstring>

#include "platform.h"

#include "etl/algorithm.h"
#include "etl/murmur3.h"

// ============================================================
// utils.h — uros 工具函数
//
// 提供三个内联工具：
//   type_id<T>()       获取类型的编译期唯一 ID（利用函数指针地址）
//   timeout_now()      根据进入时刻与剩余超时计算"此刻剩余超时"
//   calc_entry_hash()  计算 topic / service 名称的 murmur3 哈希，
//                      作为全局唯一 ID
// ============================================================

namespace uros {

// type_id<T>(): 利用每个模板实例化产生的函数地址作为类型的唯一运行时标识。
// 不依赖 RTTI，适用于嵌入式平台。
template <typename T> constexpr type_id_t type_id() {
  return reinterpret_cast<const type_id_t>(&type_id<T>);
}

// timeout_now(): 将原始超时时间换算成当前调用处的剩余超时。
// 入参：
//   init_timeout_ms  — 原始超时时间（ms），负值表示永远等待
//   init_stamp_ms    — 进入阻塞路径时记录的时间戳（now_ms()）
// 返回：max(0, init_timeout_ms - elapsed)，但不超过 init_timeout_ms。
inline int timeout_now(const int &init_timeout_ms,
                       const int64_t &init_stamp_ms) {
  return etl::min(init_timeout_ms,
                  etl::max(init_timeout_ms - int(now_ms() - init_stamp_ms), 0));
}

// calc_entry_hash(): 计算字符串名称的 murmur3 哈希值，
// 作为 topic / service 的全局唯一 32 位 ID 存入 MsgMeta::entry_hash。
inline uint32_t calc_entry_hash(const char *entry_name) {
  const uint8_t *begin = reinterpret_cast<const uint8_t *>(entry_name);
  const uint8_t *end = begin + std::strlen(entry_name);
  etl::murmur3<uint32_t> hash(begin, end);
  return hash.value();
}

} // namespace uros
