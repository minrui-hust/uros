#pragma once

#include <fcntl.h>
#include <iomanip>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>

#include "transport_remote_host_uart.h"

uint32_t total_recv_len = 0;
uint32_t unpack_success = 0;
uint32_t repeat_unpacke = 0;
uint32_t ret = 0;
uint32_t buf_overrun = 0;

uint8_t cur_seq_id = 0;
uint8_t last_seq_id = 0;
bool first_recv_seq_id = 1;
uint32_t seq_id_girht = 0;
uint32_t seq_id_err = 0;

namespace uros {

inline void TransportRemoteHostUart::initHostUart(const char *dev_name) {
  //打开设备
  fd_ = open(dev_name, O_RDWR | O_NOCTTY);
  if (fd_ < 0) {
    perror("open");
  }

  // 配置串口
  struct termios tty;
  tcgetattr(fd_, &tty);
  cfsetospeed(&tty, B115200);
  cfsetispeed(&tty, B115200);
  tty.c_cflag |= (CLOCAL | CREAD); // 本地连接 + 启用接收
  tty.c_cflag &= ~PARENB;          // 无校验
  tty.c_cflag &= ~CSTOPB;          // 1停止位
  tty.c_cflag &= ~CSIZE;           // 清除数据位掩码
  tty.c_cflag |= CS8;              // 8数据位
  tty.c_lflag &= ~(
      ICANON | ECHO | ECHOE |
      ISIG); //禁用规范模式
             //,禁用输入字符的回显,禁用擦除字符的回显,禁用中断、退出等特殊信号字符处理
  tty.c_iflag &= ~(IXON | IXOFF | IXANY); //禁用软件输出/输入流控制
  tty.c_oflag &= ~OPOST; //禁用输出处理 - 输出原始数据，不进行任何转换

  // **关闭输入输出中的回车/换行转换**:cite[6]
  tty.c_iflag &= ~(ICRNL | INLCR); // 禁止CR->NL, NL->CR转换
  tty.c_oflag &= ~(ONLCR | OCRNL); // 禁止输出中的NL->CR-NL映射等

  // 设置读取超时，避免阻塞
  tty.c_cc[VMIN] = 1;  // 至少读取1个字符才返回
  tty.c_cc[VTIME] = 0; // 无限等待，不超时

  tcsetattr(fd_, TCSANOW, &tty);

  // 清空缓冲区
  tcflush(fd_, TCIOFLUSH);
}

inline int TransportRemoteHostUart::send(const void *data, size_t len, int prio,
                                         int timeout_ms) {
  UROS_PRINT("transport_remote_socket.send: %zu\n", len);
  //封包
  uint32_t packer_size =
      packer_.buildPacket((const uint8_t *)data, len, send_buf_, MAX_SIZE);
  return write(fd_, send_buf_, packer_size);
  tcdrain(fd_);
}

inline int TransportRemoteHostUart::recv(void *data, size_t len, int *prio,
                                         int timeout_ms) {
  //直接读packer的buf
  int32_t read_buf_len = packer_.read_buf((uint8_t *)data, len);
  if (read_buf_len > 0) {
    uint8_t *byte_data = static_cast<uint8_t *>(data);
    std::cout << std::hex << std::uppercase << std::setfill('0');
    for (int32_t i = 0; i < read_buf_len; ++i) {
      std::cout << std::setw(2) << static_cast<int>(byte_data[i]);
      if (i < read_buf_len - 1) {
        std::cout << " ";
      }
    }
    std::cout << std::dec << std::nouppercase << std::endl;

    ret++;
    return read_buf_len;
  }

  bool get_packer = 0;
  uint32_t ret_len = 0;
  while (!get_packer) {

    int received = read(fd_, recv_buf_, len);
    total_recv_len += received;

    if (received < 0) {
      std::cout << "read error': " << std::endl;
    } else {
      //解包
      for (uint16_t i = 0; i < received; i++) {
        if (packer_.processByte(recv_buf_[i])) {
          unpack_success++;
          // std::cout << "unpack success: " << unpack_success << std::endl;

          // std::cout << "recv seq id: "
          //           << static_cast<int>(packer_.current_seq_id) << std::endl;

          if (first_recv_seq_id) {
            last_seq_id = packer_.current_seq_id;
            first_recv_seq_id = 0;
            seq_id_girht++;
          } else {
            cur_seq_id = packer_.current_seq_id;
            if (((cur_seq_id == 0) && (last_seq_id == 255)) ||
                ((cur_seq_id - last_seq_id) == 1)) {
              seq_id_girht++;
              std::cout << "right:" << seq_id_girht << std::endl;
            } else {
              seq_id_err++;
              std::cout << "seq_id err!!! :  " << seq_id_err << std::endl;
              std::cout << "cur_seq_id: " << cur_seq_id
                        << "last_seq_id: " << last_seq_id << std::endl;
            }
            last_seq_id = cur_seq_id;
          }

          if (get_packer == 0) {
            memcpy(data, packer_.data_buffer, packer_.expected_length);
            ret_len = packer_.expected_length;
            get_packer = 1;
          } else {
            repeat_unpacke++;
            std::cout << "repeat_unpacke:" << repeat_unpacke << std::endl;

            if (packer_.write_buf(packer_.data_buffer,
                                  packer_.expected_length) == -1) {
              buf_overrun++;
            }
          }

          packer_.reset();
        }
      }

      if (!get_packer) {
        std::cout << "unpack false!!!!!!!!!" << std::endl;
      } else {
        //   //解包成功
        //   std::cout << std::hex << std::setfill('0');
        //   for (size_t i = 0; i < (received - 4); ++i) {
        //     std::cout << "0x" << std::setw(2)
        //               << static_cast<int>(recv_buf_[i + 2]) << " ";
        //   }
        //   std::cout << std::dec << std::endl; // 恢复十进制
      }
    }
  }

  ret++;
  UROS_PRINT("transport_remote_socket.recv: %d\n", received);
  return ret_len;
}

} // namespace uros
