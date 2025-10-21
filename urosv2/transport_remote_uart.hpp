#pragma once

#include "transport_remote_uart.h"
extern void MyPrintf(const char *format, ...);
// 数据包处理回调函数
void handlePacket(const uint8_t *data, uint16_t length) {
  printf("收到完整数据包，长度: %u, 数据: ", length);
  for (uint16_t i = 0; i < length; i++) {
    printf("%02X ", data[i]);
  }
  printf("\n");
}
uint32_t send_count = 0;
uint32_t total_send_len = 0;
uint32_t recv_count = 0;
uint32_t unpack_count = 0;
uint32_t unpacke_err = 0;
uint32_t repeat_unpacke = 0;
uint32_t ret = 0;
uint32_t buf_overrun = 0;

uint8_t arr[1024] = {0};
uint32_t arr_index = 0;

uint8_t cur_seq_id = 0;
uint8_t last_seq_id = 0;
bool first_recv_seq_id = 1;
uint32_t seq_id_right = 0;
uint32_t seq_id_err = 0;

namespace uros {

inline void TransportRemoteUart::initUart(const char *uart_name) {
  //打开设备
  uart_ = Device::open<Uart>(uart_name);
}

inline int TransportRemoteUart::send(const void *data, size_t len, int prio,
                                     int timeout_ms) {
  UROS_PRINT("transport_remote_socket.send: %zu\n", len);
  send_count++;
  //封包
  uint32_t packer_size =
      packer_.buildPacket((const uint8_t *)data, len, send_buf_, MAX_SIZE);
  uint32_t send_len =
      uart_->send((const char *)send_buf_, packer_size, timeout_ms);
  total_send_len += send_len;
  return send_len;
}

inline int TransportRemoteUart::recv(void *data, size_t len, int *prio,
                                     int timeout_ms) {
  //直接读packer的buf
  int32_t read_buf_len = packer_.read_buf((uint8_t *)data, len);
  if (read_buf_len > 0) {
    ret++;
    return read_buf_len;
  }

  bool get_packer = 0;
  uint32_t ret_len = 0;
  while (!get_packer) {
    int received = uart_->recv((char *)recv_buf_, len, timeout_ms);

    recv_count += received;
    //解包
    for (uint16_t i = 0; i < received; i++) {
      if (packer_.processByte(recv_buf_[i])) {

        unpack_count++;
        // MyPrintf("p%d\n", unpack_count);

        if (first_recv_seq_id) {
          last_seq_id = packer_.current_seq_id;
          first_recv_seq_id = 0;
          seq_id_right++;
        } else {
          cur_seq_id = packer_.current_seq_id;
          if (((cur_seq_id == 0) && (last_seq_id == 255)) ||
              ((cur_seq_id - last_seq_id) == 1)) {
            seq_id_right++;
          } else {
            seq_id_err++;
            // MyPrintf("seq_id err !!!cur:% d, last:%d\n", cur_seq_id,
            //          last_seq_id);
          }
          last_seq_id = cur_seq_id;
        }

        if (get_packer == 0) {
          memcpy(data, packer_.data_buffer, packer_.expected_length);
          ret_len = packer_.expected_length;
          get_packer = 1;
        } else {
          repeat_unpacke++;
          if (packer_.write_buf(packer_.data_buffer, packer_.expected_length) ==
              -1) {
            buf_overrun++;
          }
        }
        packer_.reset();
      }
    }
    if (!get_packer) {
      unpacke_err++;
      if ((arr_index + received) < 1024) {
        memcpy(arr + arr_index, recv_buf_, received);
        arr_index += received;
      }
      MyPrintf("unpack err: %d\n", unpacke_err);
    }
  }

  UROS_PRINT("transport_remote_socket.recv: %d\n", received);
  ret++;
  return ret_len;
}

} // namespace uros
