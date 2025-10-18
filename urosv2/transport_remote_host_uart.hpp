#pragma once

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>

#include "transport_remote_host_uart.h"

namespace uros {

inline void TransportRemoteHostUart::initHostUart(const char *drv_name) {
  //打开设备
  fd_ = open(drv_name, O_RDWR | O_NOCTTY);
  if (fd_ < 0) {
    perror("open");
  }

  // 配置串口
  struct termios tty;
  tcgetattr(fd_, &tty);
  cfsetospeed(&tty, B115200);
  cfsetispeed(&tty, B115200);
  tty.c_cflag |= (CLOCAL | CREAD);
  tty.c_cflag &= ~PARENB; // 无校验
  tty.c_cflag &= ~CSTOPB; // 1停止位
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8; // 8数据位
  tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  tty.c_iflag &= ~(IXON | IXOFF | IXANY);
  tty.c_oflag &= ~OPOST;

  // 设置读取超时，避免阻塞
  tty.c_cc[VMIN] = 0;  // 非阻塞读取
  tty.c_cc[VTIME] = 1; // 0.1秒超时

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
}

inline int TransportRemoteHostUart::recv(void *data, size_t len, int *prio,
                                         int timeout_ms) {

  int received = read(fd_, recv_buf_, len);

  //解包
  uint32_t ret_len = 0;
  for (uint16_t i = 0; i < received; i++) {
    if (packer_.processByte(recv_buf_[i])) {
      memcpy(data, packer_.packet_buffer_, packer_.expected_length_);
      ret_len = packer_.expected_length_;
      packer_.reset();
    }
  }

  UROS_PRINT("transport_remote_socket.recv: %d\n", received);
  return ret_len;
}

} // namespace uros
