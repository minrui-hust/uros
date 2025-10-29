#pragma once

#include <hb_ipcf_hal.h>
#include <ipcf_hal_errno.h>
#include <stdint.h>

#include <mutex>

#include "ipc.h"
#include "transport.h"

namespace uros {

#define IPC_CHANNEL (8u)
#define SMP_CFG_FILE "/etc/ipcfhal_config.json"
#define IPC_CHAN_NAME_MAXLEN (128u)

struct TransportHostIpc : public TransportBase {
  void initIpc(uint32_t instance, uint32_t id);

  int send(const void* data, size_t len, int prio, int timeout_ms) override;

  // blocking recv, thread safe is NOT required
  // UDP recvfrom always receives one complete datagram (packet)
  // Returns the number of bytes in the received packet, or -1 on error
  int recv(void* data, size_t len, int* prio, int timeout_ms) override;

  ~TransportHostIpc();

 private:
  bool enabled_ = false;
  std::mutex tx_mutexs_;
  std::mutex rx_mutexs_;
  ipcfhal_chan_t ch_;
};

}  // namespace uros
