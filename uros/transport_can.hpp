#pragma once

#include "transport_can.h"

uint32_t can_send_count = 0;
uint32_t can_total_send_len = 0;
uint32_t can_recv_count = 0;
uint32_t can_total_recv_len = 0;
uint32_t can_ret_count = 0;

namespace uros {
// CAN FD 支持的固定有效载荷长度集合 (DLCs)
static const uint8_t CANFD_DLCS[16] = {0, 1,  2,  3,  4,  5,  6,  7,
                                       8, 12, 16, 20, 24, 32, 48, 64};
static const uint8_t MAX_CANFD_LEN = 64;

uint8_t get_canfd_padded_length_c(uint16_t len);

inline void TransportCan::initCan(const char* can_name, int can_id) {
  // 打开can设备
  can_ = Device::open<CanV2>(can_name);

  // 配置滤波器
  can_->addFilterWithMask(can_id, 0x7ff, 0);
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
    cur_send_len = get_canfd_padded_length_c(to_send_len);
    to_send_len -= cur_send_len;

    int32_t send_len =
        can_->send((void*)(send_buf_ + offset), cur_send_len, prio, timeout_ms);
    offset += send_len;
    can_total_send_len += send_len;
  }
  can_send_count++;
  return len;

  // uint32_t send_len = can_->send((void*)data, len, prio, timeout_ms);
  // can_total_send_len += send_len;
  // return send_len;

  // 封包
  // Header send_header = packer_.buildPacket2((const uint8_t*)data, len);
  // uint32_t send_len =
  //     uart_->send((const char*)&send_header, sizeof(Header), timeout_ms);
  // send_len += uart_->send((const char*)data, len, timeout_ms);
  // total_send_len += send_len;
  // return send_len;
}

inline int TransportCan::recv(void* data, size_t len, int* prio,
                              int timeout_ms) {
  can_recv_count++;
  // 直接读packer的buf
  int32_t read_buf_len = packer_.read_buf((uint8_t*)data, len);
  if (read_buf_len > 0) {
    can_ret_count++;
    can_total_recv_len += read_buf_len;
    return read_buf_len;
  }

  CanMsg can_msg = {0};
  while (1) {
    int received = can_->recv(&can_msg, 0, timeout_ms);

    // 解包
    packer_.putDataOneByte(can_msg.data, can_msg.len);
    // 获取完整数据
    read_buf_len = packer_.getData((uint8_t*)data, len);
    if (read_buf_len > 0) {
      *prio = can_msg.id;

      can_ret_count++;
      can_total_recv_len += read_buf_len;

      return read_buf_len;
    }
  }

  // CanMsg can_msg = {0};
  // int received = can_->recv(&can_msg, 0, timeout_ms);
  // *prio = can_msg.id;
  // memcpy(data, can_msg.data, can_msg.len);
  // can_total_recv_len += can_msg.len;

  // // printf("transport_can.recv len: %d\n", received);
  // return can_msg.len;
}

/**
 * @brief 查找大于或等于给定长度的最小 CAN FD 有效载荷长度。
 * * @param len 输入的原始数据长度 (最大支持 64)。
 * @return uint8_t 补齐后的 CAN FD 有效载荷长度
 * (0-64)，如果输入超过64，则返回64。
 */
uint8_t get_canfd_padded_length_c(uint16_t len) {
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
