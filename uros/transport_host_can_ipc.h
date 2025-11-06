#pragma once

#include <hb_ipcf_hal.h>
#include <ipcf_hal_errno.h>
#include <stdint.h>

#include <mutex>

#include "ipc.h"
#include "packer.h"
#include "transport.h"

namespace uros {

#define IPC_CHANNEL (8u)
#define SMP_CFG_FILE "/etc/ipcfhal_config.json"
#define IPC_CHAN_NAME_MAXLEN (128u)

struct TransportHostCanIpc : public TransportBase {
  void initCanIpc(uint32_t instance, uint32_t id);

  int send(const void* data, size_t len, int prio, int timeout_ms) override;

  // blocking recv, thread safe is NOT required
  // UDP recvfrom always receives one complete datagram (packet)
  // Returns the number of bytes in the received packet, or -1 on error
  int recv(void* data, size_t len, int* prio, int timeout_ms) override;

  ~TransportHostCanIpc();

 private:
  bool enabled_ = false;
  std::mutex tx_mutexs_;
  std::mutex rx_mutexs_;
  ipcfhal_chan_t ch_;

  static const uint16_t MAX_SIZE = 512;
  uint8_t recv_buf_[MAX_SIZE] = {0};
  uint8_t send_buf_[MAX_SIZE] = {0};

  PacketParser packer_;
};

}  // namespace uros
