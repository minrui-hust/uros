#pragma once

#include "transport_can.h"

uint32_t transport_can_send_count = 0;
uint32_t transport_can_total_send_len = 0;
uint32_t transport_can_total_recv_len = 0;
uint32_t transport_can_recv_count = 0;

uint32_t can_recv_count = 0;
uint32_t can_recv_len = 0;

namespace uros {
// CAN FD 支持的固定有效载荷长度集合 (DLCs)
static const uint8_t CANFD_DLCS[16] = {0, 1,  2,  3,  4,  5,  6,  7,
                                       8, 12, 16, 20, 24, 32, 48, 64};
static const uint8_t MAX_CANFD_LEN = 64;

uint8_t get_canfd_padded_length(uint16_t len);

inline void TransportCan::initCan(const char* can_name) {
  // 打开can设备
  can_ = Device::open<CanV2>(can_name);

  // 配置滤波器
  // can_->addFilterWithMask(can_id, 0x7ff, 0);
  can_->addFilterWithRange(0x000, 0x0ff, 0);
}

inline int TransportCan::send(const void* data, size_t len, int prio,
                              int timeout_ms) {
  prio = (prio > 0x7ff) ? 0x7ff : prio;
  // printf("transport_can.send len: %d\n", len);
  // 封包
  int32_t packer_size =
      packer_.buildPacket((const uint8_t*)data, len, send_buf_, MAX_SIZE);

  int32_t to_send_len = packer_size;
  int32_t cur_send_len = 0;
  int32_t offset = 0;

  while (to_send_len > 0) {
    cur_send_len = get_canfd_padded_length(to_send_len);
    to_send_len -= cur_send_len;

    int32_t send_len =
        can_->send((void*)(send_buf_ + offset), cur_send_len, prio, timeout_ms);
    offset += send_len;
    transport_can_total_send_len += send_len;
  }
  transport_can_send_count++;
  return len;
}

inline int TransportCan::recv(void* data, size_t len, int* prio,
                              int timeout_ms) {
  // 直接读packer的buf
  int32_t read_buf_len = packer_.read_buf((uint8_t*)data, len);
  if (read_buf_len > 0) {
    transport_can_recv_count++;
    transport_can_total_recv_len += read_buf_len;
    return read_buf_len;
  }

  CanMsg can_msg = {0};
  while (1) {
    int received = can_->recv(&can_msg, 0, timeout_ms);
    can_recv_count++;
    can_recv_len += received;

    // 解包
    packer_.putData(can_msg.data, can_msg.len);
    // 获取完整数据
    read_buf_len = packer_.getData((uint8_t*)data, len);
    if (read_buf_len > 0) {
      *prio = can_msg.id;

      transport_can_recv_count++;
      transport_can_total_recv_len += read_buf_len;

      return read_buf_len;
    }
  }
}

/**
 * @brief 查找大于或等于给定长度的最小 CAN FD 有效载荷长度
 * @param len 输入的原始数据长度
 * @return uint8_t 补齐后的 CAN FD 有效载荷长度(0-64)，
 *         如果输入超过64，则返回64。
 */
uint8_t get_canfd_padded_length(uint16_t len) {
  size_t i;

  // 1. 边界条件检查：零长度
  if (len == 0) {
    return 0;
  }

  // 2. 边界条件检查：超过最大值
  if (len > MAX_CANFD_LEN) {
    return MAX_CANFD_LEN;
  }

  // 3. 循环遍历 CAN FD 长度集合，查找第一个 >= len 的值
  for (i = 0; i < 16; i++) {
    if (CANFD_DLCS[i] >= len) {
      return CANFD_DLCS[i];
    }
  }

  // 理论上由于 len <= 64 且 64 位于数组中，这段代码不会被执行。
  // 作为一个安全措施，返回最大值。
  return MAX_CANFD_LEN;
}
}  // namespace uros
