#pragma once

#include "transport_host_ipc.h"

namespace uros {

inline void TransportHostIpc::initIpc(void) {
  int ret = 0;
  char chan_name[IPC_CHAN_NAME_MAXLEN] = {'\0'};

  if(enabled_ == true){
    return;
  }

  for (int i = 0; i < IPC_CHANNEL; i++) {
    //1. get info from json
    snprintf(chan_name, IPC_CHAN_NAME_MAXLEN, "cpu2mcu_ins0ch%d", i);
    ret = hb_ipcfhal_getchan_byjson(chan_name, &ch_[i], SMP_CFG_FILE);
    if (ret < 0) {/* return channel id*/
      UROS_PRINT("%s not found, ret %d\n", chan_name, ret);
      return;
    }
    //2. init channel
    ret = hb_ipcfhal_init(&ch_[i]);
    if (ret < 0) {
      UROS_PRINT("%s init failed, ret %d\n", ch_[i].name, ret);
      hb_ipcfhal_deinit(&ch_[i]);
      return;
	  }
    ret = hb_ipcfhal_config(&ch_[i]);
    if (ret < 0) {
      UROS_PRINT("%s config failed, ret %d\n", ch_[i].name, ret);
      hb_ipcfhal_deinit(&ch_[i]);
      return;
    }
  }

  if(ret == 0) {
    enabled_ = true;
  }

  return;
}

TransportHostIpc::~TransportHostIpc() {
  for (int i = 0; i < IPC_CHANNEL; i++) {
    hb_ipcfhal_deinit(&ch_[i]);
  }
}

inline int TransportHostIpc::send(const void *data, size_t len, int prio,
                              int timeout_ms) {
  int ret = 0;
  if ((data == NULL) || (len > 128) || (timeout_ms < -1)) {
    return -1;
  }
  prio %= 8;

  tx_mutexs_[prio].lock();
  ret = hb_ipcfhal_send(reinterpret_cast<const uint8_t *>(data), len, &ch_[prio]);
  tx_mutexs_[prio].unlock();
	if (ret != len) {/*return data_len*/
		UROS_PRINT("Ins[%u]Ch[%u]TxCnt send failed\n",
			ch_[prio].instance, ch_[prio].id);
	}
  return ret;
}

inline int TransportHostIpc::recv(void *data, size_t len, int *prio,
                              int timeout_ms) {
  int ret = 0;
  int prio_recv = *prio % 8;
  if ((data == NULL) || (len <= 0) || (timeout_ms < -1)) {
    return -1;
  }
  rx_mutexs_[prio_recv].lock();
	ret = hb_ipcfhal_recv(reinterpret_cast<uint8_t *>(data), len, timeout_ms, &ch_[prio_recv]);
  rx_mutexs_[prio_recv].unlock();
	if (ret < 0) {
		if (ret == -IPCF_HAL_E_TIMEOUT)
			UROS_PRINT("no recvice data\n");
		else
			UROS_PRINT("Ins[%u]Ch[%u] recv failed: %d\n",
				ch_[prio_recv].instance, ch_[prio_recv].id, ret);
	}

  return ret;
}

} // namespace uros
