#pragma once

#include "transport_uart.h"

namespace uros {

uint32_t send_count = 0;
uint32_t total_send_len = 0;
uint32_t recv_count = 0;
uint32_t unpack_count = 0;
uint32_t unpacke_err = 0;
uint32_t repeat_unpacke = 0;
uint32_t ret = 0;
uint32_t buf_overrun = 0;

inline void TransportUart::initUart(const char* uart_name) {
  // 打开设备
  uart_ = Device::open<Uart>(uart_name);
}

inline int TransportUart::send(const void* data, size_t len, int prio,
                               int timeout_ms) {
  send_count++;
  // 封包
  Header send_header = packer_.buildPacket2((const uint8_t*)data, len);
  uint32_t send_len =
      uart_->send((const char*)&send_header, sizeof(Header), timeout_ms);
  send_len += uart_->send((const char*)data, len, timeout_ms);
  total_send_len += send_len;
  return send_len;
}

inline int TransportUart::recv(void* data, size_t len, int* prio,
                               int timeout_ms) {
  // 直接读packer的buf
  int32_t read_buf_len = packer_.read_buf((uint8_t*)data, len);
  if (read_buf_len > 0) {
    ret++;
    return read_buf_len;
  }

  uint32_t ret_len = 0;
  while (1) {
    int received = uart_->recv((char*)recv_buf_, len, timeout_ms);

    recv_count += received;

    // 解包
    packer_.putData(recv_buf_, received);
    // 获取完整数据
    read_buf_len = packer_.getData((uint8_t*)data, len);
    if (read_buf_len > 0) {
      ret++;
      return read_buf_len;
    }
  }
}

/*
inline int TransportUart::recv(void *data, size_t len, int *prio,
                               int timeout_ms) {
  //直接读packer的buf
  int32_t read_buf_len = packer_.read_buf((uint8_t *)data, len);
  if (read_buf_len > 0) {
    ret++;
    return read_buf_len;
  }

  int32_t get_packer = 0;
  uint32_t ret_len = 0;
  while (!get_packer) {
    int received = uart_->recv((char *)recv_buf_, len, timeout_ms);

    recv_count += received;
    //解包
    get_packer += packer_.processBuffer(recv_buf_, received, (uint8_t *)data,
                                        len, &ret_len);
    unpack_count += get_packer;
  }

  UROS_PRINT("transport_remote_socket.recv: %d\n", received);
  ret++;
  return ret_len;
}
  */

// inline int TransportUart::recv(void *data, size_t len, int *prio,
//                                int timeout_ms) {
//   //直接读packer的buf
//   int32_t read_buf_len = packer_.read_buf((uint8_t *)data, len);
//   if (read_buf_len > 0) {
//     ret++;
//     return read_buf_len;
//   }

//   bool get_packer = 0;
//   uint32_t ret_len = 0;
//   while (!get_packer) {
//     int received = uart_->recv((char *)recv_buf_, len, timeout_ms);

//     recv_count += received;
//     //解包
//     for (uint16_t i = 0; i < received; i++) {
//       if (packer_.processByte(recv_buf_[i])) {

//         unpack_count++;
//         // MyPrintf("p%d\n", unpack_count);

//         if (first_recv_seq_id) {
//           last_seq_id = packer_.header.seq_id;
//           first_recv_seq_id = 0;
//           seq_id_right++;
//         } else {
//           cur_seq_id = packer_.header.seq_id;
//           if (((cur_seq_id == 0) && (last_seq_id == 255)) ||
//               ((cur_seq_id - last_seq_id) == 1)) {
//             seq_id_right++;
//           } else {
//             seq_id_err++;
//             // MyPrintf("seq_id err !!!cur:% d, last:%d\n", cur_seq_id,
//             //          last_seq_id);
//           }
//           last_seq_id = cur_seq_id;
//         }

//         if (get_packer == 0) {
//           memcpy(data, packer_.data_buffer, packer_.header.len);
//           ret_len = packer_.header.len;
//           get_packer = 1;
//         } else {
//           repeat_unpacke++;
//           if (packer_.write_buf(packer_.data_buffer, packer_.header.len) ==
//               -1) {
//             buf_overrun++;
//           }
//         }
//         packer_.reset();
//       }
//     }
//     if (!get_packer) {
//       unpacke_err++;
//       if ((arr_index + received) < 1024) {
//         memcpy(arr + arr_index, recv_buf_, received);
//         arr_index += received;
//       }
//       // MyPrintf("unpack err: %d\n", unpacke_err);
//     }
//   }

//   UROS_PRINT("transport_remote_socket.recv: %d\n", received);
//   ret++;
//   return ret_len;
// }

}  // namespace uros
