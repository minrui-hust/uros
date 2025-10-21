#pragma once

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>

#include "transport_host_uart.h"

namespace uros {

inline void TransportHostUart::initHostUart() {
  // 打开设备
  fd_ = open("/dev/ttyACM0", O_RDWR | O_NOCTTY);
  //   if (fd < 0) {
  //     perror("open");
  //     return 1;
  //   }

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

inline int TransportHostUart::send(const void *data, size_t len, int prio,
                                   int timeout_ms) {
  UROS_PRINT("transport_host_uart.send: %zu\n", len);
  return write(fd_, data, len);
}

inline int TransportHostUart::recv(void *data, size_t len, int *prio,
                                   int timeout_ms) {

  int received = read(fd_, data, len);

  UROS_PRINT("transport_host_uart.recv: %d\n", received);
  return received;
}

} // namespace uros
