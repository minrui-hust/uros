#pragma once

#include "ipc.h"
#include <stdint.h>
#include <mutex>
#include <hb_ipcf_hal.h>
#include <ipcf_hal_errno.h>
#include "transport.h"

namespace uros {

#define IPC_CHANNEL           (8u)
#define SMP_CFG_FILE	        "/etc/ipcfhal_config.json"
#define IPC_CHAN_NAME_MAXLEN  (128u)

struct TransportHostIpc : public TransportBase {

  void initIpc(void);

  int send(const void *data, size_t len, int prio, int timeout_ms) override;

  // blocking recv, thread safe is NOT required
  // UDP recvfrom always receives one complete datagram (packet)
  // Returns the number of bytes in the received packet, or -1 on error
  int recv(void *data, size_t len, int *prio, int timeout_ms) override;

  ~TransportHostIpc();

private:
  bool enabled_ = false;
  std::mutex tx_mutexs_[IPC_CHANNEL];
  std::mutex rx_mutexs_[IPC_CHANNEL];
  ipcfhal_chan_t ch_[IPC_CHANNEL];
};

} // namespace uros
