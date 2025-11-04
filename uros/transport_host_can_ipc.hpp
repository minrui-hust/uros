#pragma once

#include "can_v2.h"
#include "transport_host_can_ipc.h"

uint32_t transportcanipc_send_count = 0;
uint32_t transportcanipc_send_len = 0;
uint32_t transportcanipc_recv_count = 0;
uint32_t transportcanipc_recv_len = 0;

namespace uros {

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
  if ((data == NULL) || (len > 128) || (timeout_ms < -1)) {
    return -1;
  }
  CanMsg can_msg = {0};
  can_msg.id = prio;
  can_msg.len = len;
  memcpy(can_msg.data, data, len);  // todo:一次最大64
  tx_mutexs_.lock();
  ret = hb_ipcfhal_send(reinterpret_cast<const uint8_t*>(&can_msg), len + 4,
                        &ch_);
  tx_mutexs_.unlock();
  if (ret != (len + 4)) { /*return data_len*/
    UROS_PRINT("Ins[%u]Ch[%u]TxCnt send failed\n", ch_.instance, ch_.id);
  }

  // printf("transportCanIpc send len %d\n", ret);
  transportcanipc_send_count++;
  transportcanipc_send_len += ret;

  return can_msg.len;
}

inline int TransportHostCanIpc::recv(void* data, size_t len, int* prio,
                                     int timeout_ms) {
  int ret = 0;
  if ((data == NULL) || (len <= 0) || (timeout_ms < -1)) {
    return -1;
  }
  CanMsg can_msg = {0};
  rx_mutexs_.lock();
  ret = hb_ipcfhal_recv(reinterpret_cast<uint8_t*>(&can_msg), sizeof(CanMsg),
                        timeout_ms, &ch_);  // 单次接收不超过64

  rx_mutexs_.unlock();
  // printf("transportCanIpc recv len %d\n", ret);
  transportcanipc_recv_count++;
  if (ret < 0) {
    if (ret == -IPCF_HAL_E_TIMEOUT)
      UROS_PRINT("no recvice data\n");
    else
      UROS_PRINT("Ins[%u]Ch[%u] recv failed: %d\n", ch_.instance, ch_.id, ret);
  } else {
    transportcanipc_recv_len += ret;
    *prio = can_msg.id;
    memcpy(data, can_msg.data, can_msg.len);
  }

  return can_msg.len;
}

}  // namespace uros
