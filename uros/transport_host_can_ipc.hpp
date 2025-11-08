#pragma once

#include "can_v2.h"
#include "transport_host_can_ipc.h"

uint32_t transportcanipc_send_count = 0;
uint32_t transportcanipc_send_len = 0;
uint32_t transportcanipc_recv_count = 0;
uint32_t transportcanipc_recv_len = 0;
uint32_t transportcanipc_enter_recv = 0;
uint32_t ipcfhal_recv_len = 0;

namespace uros {

// CAN FD 支持的固定有效载荷长度集合 (DLCs)
static const uint8_t CANFD_DLCS[16] = {0, 1,  2,  3,  4,  5,  6,  7,
                                       8, 12, 16, 20, 24, 32, 48, 64};
static const uint8_t MAX_CANFD_LEN = 64;

uint8_t get_canfd_padded_length(uint16_t len);

inline void TransportHostCanIpc::initCanIpc(uint32_t instance, uint32_t id) {
  int ret = 0;
  char chan_name[IPC_CHAN_NAME_MAXLEN] = {'\0'};

  if (enabled_ == true) {
    return;
  }

  // 1. get info from json
  snprintf(chan_name, IPC_CHAN_NAME_MAXLEN, "cpu2mcu_ins%dch%d", instance, id);
  ret = hb_ipcfhal_getchan_byjson(chan_name, &ch_, SMP_CFG_FILE);
  if (ret < 0) { /* return channel id*/
    UROS_PRINT("%s not found, ret %d\n", chan_name, ret);
    return;
  }
  // 2. init channel
  ret = hb_ipcfhal_init(&ch_);
  if (ret < 0) {
    UROS_PRINT("%s init failed, ret %d\n", ch_.name, ret);
    hb_ipcfhal_deinit(&ch_);
    return;
  }
  ret = hb_ipcfhal_config(&ch_);
  if (ret < 0) {
    UROS_PRINT("%s config failed, ret %d\n", ch_.name, ret);
    hb_ipcfhal_deinit(&ch_);
    return;
  }

  if (ret == 0) {
    enabled_ = true;
  }

  return;
}

TransportHostCanIpc::~TransportHostCanIpc() { hb_ipcfhal_deinit(&ch_); }

inline int TransportHostCanIpc::send(const void* data, size_t len, int prio,
                                     int timeout_ms) {
  int ret = 0;
  if ((data == NULL) || (len > 256) || (timeout_ms < -1)) {
    return -1;
  }
  prio = (prio > 0x7ff) ? 0x7ff : prio;

  // 封包
  int32_t packer_size =
      packer_.buildPacket((const uint8_t*)data, len, send_buf_, MAX_SIZE);
  // printf("packer_size:%d\n", packer_size);

  int32_t to_send_len = packer_size;
  int32_t cur_send_len = 0;
  int32_t offset = 0;
  CanMsg can_msg = {0};

  while (to_send_len > 0) {
    cur_send_len = get_canfd_padded_length(to_send_len);
    to_send_len -= cur_send_len;

    can_msg.id = prio;
    can_msg.len = cur_send_len;
    memcpy(can_msg.data, (send_buf_ + offset), can_msg.len);
    offset += can_msg.len;

    tx_mutexs_.lock();
    ret = hb_ipcfhal_send(reinterpret_cast<const uint8_t*>(&can_msg),
                          can_msg.len + 4, &ch_);
    tx_mutexs_.unlock();
    // printf("ipc send len:%d\n", ret);
    if (ret != (can_msg.len + 4)) { /*return data_len*/
      UROS_PRINT("Ins[%u]Ch[%u]TxCnt send failed\n", ch_.instance, ch_.id);
    }
    transportcanipc_send_len += ret;
  }

  // printf("transportCanIpc send len %d\n", ret);
  transportcanipc_send_count++;
  // transportcanipc_send_len += len;

  return len;
}

inline int TransportHostCanIpc::recv(void* data, size_t len, int* prio,
                                     int timeout_ms) {
  transportcanipc_enter_recv++;
  int ret = 0;
  if ((data == NULL) || (len <= 0) || (timeout_ms < -1)) {
    return -1;
  }
  // 直接读packer的buf
  int32_t read_buf_len = packer_.read_buf((uint8_t*)data, len);
  if (read_buf_len > 0) {
    transportcanipc_recv_count++;
    transportcanipc_recv_len += read_buf_len;
    return read_buf_len;
  }
  static uint32_t last_seq = 0;
  while (1) {
    CanMsg can_msg = {0};
    rx_mutexs_.lock();
    ret = hb_ipcfhal_recv(reinterpret_cast<uint8_t*>(&can_msg), sizeof(CanMsg),
                          timeout_ms, &ch_);
    rx_mutexs_.unlock();

    if (ret < 0) {
      if (ret == -IPCF_HAL_E_TIMEOUT)
        UROS_PRINT("no recvice data\n");
      else
        UROS_PRINT("Ins[%u]Ch[%u] recv failed: %d\n", ch_.instance, ch_.id,
                   ret);
    } else {
      // printf("seq:%d\n", can_msg.data[0]);
      // if ((can_msg.data[0] - last_seq) != 1) {
      //   if (last_seq == 255 && can_msg.data[0] == 0) {
      //     /* code */
      //   } else {
      //     printf("seq:%d,last:%d\n", can_msg.data[0], last_seq);
      //   }
      // }
      // last_seq = can_msg.data[0];

      ipcfhal_recv_len += ret;
      // 解包
      packer_.putData(can_msg.data, can_msg.len);
      // 获取完整数据
      read_buf_len = packer_.getData((uint8_t*)data, len);
      if (read_buf_len > 0) {
        *prio = can_msg.id;

        transportcanipc_recv_count++;
        transportcanipc_recv_len += read_buf_len;
        return read_buf_len;
      }
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

// inline int TransportHostCanIpc::send(const void* data, size_t len, int prio,
//                                      int timeout_ms) {
//   int ret = 0;
//   if ((data == NULL) || (len > 128) || (timeout_ms < -1)) {
//     return -1;
//   }
//   CanMsg can_msg = {0};
//   can_msg.id = prio;
//   can_msg.len = len;
//   memcpy(can_msg.data, data, len);  // todo:一次最大64
//   tx_mutexs_.lock();
//   ret = hb_ipcfhal_send(reinterpret_cast<const uint8_t*>(&can_msg), len + 4,
//                         &ch_);
//   tx_mutexs_.unlock();
//   if (ret != (len + 4)) { /*return data_len*/
//     UROS_PRINT("Ins[%u]Ch[%u]TxCnt send failed\n", ch_.instance, ch_.id);
//   }

//   // printf("transportCanIpc send len %d\n", ret);
//   transportcanipc_send_count++;
//   transportcanipc_send_len += ret;

//   return can_msg.len;
// }

// inline int TransportHostCanIpc::recv(void* data, size_t len, int* prio,
//                                      int timeout_ms) {
//   int ret = 0;
//   if ((data == NULL) || (len <= 0) || (timeout_ms < -1)) {
//     return -1;
//   }
//   CanMsg can_msg = {0};
//   rx_mutexs_.lock();
//   ret = hb_ipcfhal_recv(reinterpret_cast<uint8_t*>(&can_msg), sizeof(CanMsg),
//                         timeout_ms, &ch_);  // 单次接收不超过64

//   rx_mutexs_.unlock();
//   // printf("transportCanIpc recv len %d\n", ret);
//   transportcanipc_recv_count++;
//   if (ret < 0) {
//     if (ret == -IPCF_HAL_E_TIMEOUT)
//       UROS_PRINT("no recvice data\n");
//     else
//       UROS_PRINT("Ins[%u]Ch[%u] recv failed: %d\n", ch_.instance, ch_.id,
//       ret);
//   } else {
//     transportcanipc_recv_len += ret;
//     *prio = can_msg.id;
//     memcpy(data, can_msg.data, can_msg.len);
//   }

//   return can_msg.len;
// }
