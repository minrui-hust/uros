#pragma once

#include "transport_remote_uart.h"

// 数据包处理回调函数
void handlePacket(const uint8_t *data, uint16_t length) {
  printf("收到完整数据包，长度: %u, 数据: ", length);
  for (uint16_t i = 0; i < length; i++) {
    printf("%02X ", data[i]);
  }
  printf("\n");
}

namespace uros {

inline void TransportRemoteUart::initUart(const char *uart_name) {
  //打开设备
  uart_ = Device::open<Uart>(uart_name);
}

inline int TransportRemoteUart::send(const void *data, size_t len, int prio,
                                     int timeout_ms) {
  UROS_PRINT("transport_remote_socket.send: %zu\n", len);

  //封包
  uint32_t packer_size =
      packer_.buildPacket((const uint8_t *)data, len, send_buf_, MAX_SIZE);

  return uart_->send((const char *)send_buf_, packer_size, timeout_ms);
}

uint32_t recv_count = 0;
uint32_t unpack_count = 0;

inline int TransportRemoteUart::recv(void *data, size_t len, int *prio,
                                     int timeout_ms) {
  int received = uart_->recv((char *)recv_buf_, len, timeout_ms);

  recv_count++;
  //解包
  uint32_t ret_len = 0;
  for (uint16_t i = 0; i < received; i++) {
    if (packer_.processByte(recv_buf_[i])) {
      memcpy(data, packer_.packet_buffer_, packer_.expected_length_);
      ret_len = packer_.expected_length_;
      unpack_count++;
      packer_.reset();
    }
  }

  UROS_PRINT("transport_remote_socket.recv: %d\n", received);
  return ret_len;
}

} // namespace uros
