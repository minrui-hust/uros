#ifndef PACKER_H
#define PACKER_H

#include <cstdio>

#include "string.h"

#define PACKER_BUF_DATA_SIZE 256
#define PACKER_BUF_SIZE 16

struct __attribute__((aligned(4))) Header {
  uint8_t preamble;
  uint8_t crc8;
  uint16_t len;
};

struct PackerBuf {
  uint32_t len;
  uint8_t data[PACKER_BUF_DATA_SIZE];
};

class PacketParser {
 public:
  // 回调函数类型定义
  typedef void (*PackerCallback)(const Header* header, const uint8_t* data,
                                 uint32_t length);

 private:
  enum ParserState {
    STATE_PREAMBLE,     // 寻找前导字节
    STATE_CRC8,         // CRC8校验码
    STATE_LENGTH_LOW,   // 长度低字节
    STATE_LENGTH_HIGH,  // 长度高字节
    STATE_PAYLOAD       // 读取数据载荷
  };

  // 内部缓冲区配置
  static const uint16_t MAX_PACKET_SIZE = 256;  // 最大数据包大小
  static const uint16_t BUFFER_SIZE = MAX_PACKET_SIZE + sizeof(Header);

  ParserState state_ = STATE_PREAMBLE;
  uint8_t expected_preamble_;  // 期望的前导字节

  Header header_ = {0};  // 包头

  uint8_t data_buffer_[BUFFER_SIZE] = {0};  // 内部数据包缓冲区
  uint32_t data_buffer_cur_len_ = 0;        // 当前已接收数据长度

  uint8_t calculated_crc8_ = 0;  // 计算得到的CRC8值
  uint32_t crc_err_ = 0;

  PackerBuf packer_buf_[PACKER_BUF_SIZE] = {0};  // 用来存放连包
  uint8_t packer_buf_start_ = 0;
  uint8_t packer_buf_end_ = 0;

  uint32_t repeat_unpacke_ = 0;
  uint32_t buf_overrun_ = 0;

  PackerCallback packer_callback_;

 public:
  // 构造函数
  PacketParser( uint8_t preamble = 0xAA, PackerCallback callback = nullptr)
      : expected_preamble_(preamble), packer_callback_(callback) {}

  // 获取最大数据包大小
  uint16_t getMaxPacketSize() const { return MAX_PACKET_SIZE; }

  // 设置前导字节
  void setPreamble(uint8_t preamble) { expected_preamble_ = preamble; }

  // 重置解析器状态
  void reset() {
    state_ = STATE_PREAMBLE;
    data_buffer_cur_len_ = 0;
    calculated_crc8_ = 0;
    memset(&header_, 0, sizeof(Header));
  }

  // CRC8计算函数（只校验数据段）
  uint8_t calculateCRC8(const uint8_t* data, uint16_t length) {
    uint8_t crc = 0x00;
    for (uint16_t i = 0; i < length; i++) {
      crc ^= data[i];
      for (int j = 0; j < 8; j++) {
        if (crc & 0x80) {
          crc = (crc << 1) ^ 0x07;
        } else {
          crc = crc << 1;
        }
      }
    }
    return crc;
  }

  // 处理接收到的字节
  bool processByte(uint8_t byte) {
    switch (state_) {
      case STATE_PREAMBLE:
        if (byte == expected_preamble_) {
          state_ = STATE_CRC8;
          header_.preamble = byte;
        }
        break;

      case STATE_CRC8:
        header_.crc8 = byte;
        state_ = STATE_LENGTH_LOW;
        break;

      case STATE_LENGTH_LOW:
        header_.len = byte;  // 先存储低字节
        state_ = STATE_LENGTH_HIGH;
        break;

      case STATE_LENGTH_HIGH:
        header_.len |= (byte << 8);

        // 验证长度是否合理
        if (header_.len > MAX_PACKET_SIZE) {
          reset();  // 长度无效，重置
          break;
        }

        // if (header_.len == 0) {
        //   // 没有数据载荷，直接进行CRC校验
        //   calculated_crc8_ = calculateCRC8(nullptr, 0);
        //   if (calculated_crc8_ == header_.crc8) {
        //     // CRC校验成功
        //     reset();
        //     return true;
        //   } else {
        //     reset();  // CRC校验失败
        //   }
        // } else {
        //   state_ = STATE_PAYLOAD;
        //   data_buffer_cur_len_ = 0;
        // }
        if (header_.len == 0) {
          reset();
        }
        break;

      case STATE_PAYLOAD:
        // 存储数据载荷
        if (data_buffer_cur_len_ < header_.len) {
          data_buffer_[data_buffer_cur_len_++] = byte;

          // 检查是否接收完所有数据
          if (data_buffer_cur_len_ >= header_.len) {
            // 计算数据段的CRC8
            calculated_crc8_ = calculateCRC8(data_buffer_, header_.len);

            if (calculated_crc8_ == header_.crc8) {
              // CRC校验成功
              return true;
            } else {
              // CRC校验失败
              crc_err_++;
              printf("crc err,header:%d,%d,%d\n", header_.preamble,
                     header_.crc8, header_.len);
              reset();
            }
          }
        } else {
          reset();  // 缓冲区溢出，重置
        }
        break;

      default:
        reset();
        break;
    }

    return false;  // 数据包解析未完成或失败
  }

  // 获取一包完整的数据
  int32_t getData(uint8_t* data, uint32_t len) { return read_buf(data, len); }

  // 传入数据进行解包
  int32_t putData(const uint8_t* recv_buf, uint32_t received) {
    for (uint16_t i = 0; i < received; i++) {
      switch (state_) {
        case STATE_PREAMBLE:
          if (recv_buf[i] == expected_preamble_) {
            state_ = STATE_CRC8;
            header_.preamble = recv_buf[i];
          }
          break;

        case STATE_CRC8:
          header_.crc8 = recv_buf[i];
          state_ = STATE_LENGTH_LOW;
          break;

        case STATE_LENGTH_LOW:
          header_.len = recv_buf[i];
          state_ = STATE_LENGTH_HIGH;
          break;

        case STATE_LENGTH_HIGH:
          header_.len |= (recv_buf[i] << 8);
          if (header_.len > MAX_PACKET_SIZE) {
            reset();
            break;
          }

          if (header_.len == 0) {
            // 没有数据载荷，直接reset
            reset();
          } else {
            state_ = STATE_PAYLOAD;
            data_buffer_cur_len_ = 0;
          }
          break;

        case STATE_PAYLOAD: {
          // 计算剩余需要的数据长度
          uint32_t remaining_data = header_.len - data_buffer_cur_len_;
          // 计算recv_buf中剩余的数据长度
          uint32_t remaining_buf = received - i;

          if (data_buffer_cur_len_ == 0 && remaining_buf >= remaining_data) {
            // 情况1：当前没有累积数据，且剩余数据足够整个包
            // 直接对recv_buf中的数据进行CRC校验
            calculated_crc8_ = calculateCRC8(recv_buf + i, header_.len);

            if (calculated_crc8_ == header_.crc8) {
              // CRC校验成功
              // unpack_success++;

              // 将解包完成的数据包存起来
              if (write_buf(recv_buf + i, header_.len) == -1) {
                buf_overrun_++;
                printf("packer buf overrun\n");
              }

              // 跳过已处理的数据
              i += header_.len - 1;  // -1 因为循环会i++

              reset();
            } else {
              // CRC校验失败
              crc_err_++;
              printf("crc_err 1:%ld\n", crc_err_);
              reset();
            }
          } else {
            // 情况2：有累积数据或剩余数据不足
            // 计算实际可以拷贝的数据量
            uint32_t copy_size = (remaining_buf < remaining_data)
                                     ? remaining_buf
                                     : remaining_data;

            // 将数据拷贝到data_buffer
            memcpy(data_buffer_ + data_buffer_cur_len_, recv_buf + i,
                   copy_size);
            data_buffer_cur_len_ += copy_size;

            // 跳过已拷贝的数据
            i += copy_size - 1;  // -1 因为循环会i++

            // 检查是否已经收到完整的数据包
            if (data_buffer_cur_len_ == header_.len) {
              // 数据已经收齐，进行CRC校验
              calculated_crc8_ = calculateCRC8(data_buffer_, header_.len);

              if (calculated_crc8_ == header_.crc8) {
                // unpack_success++;
                // printf("data_buffer_ crc right\n");

                if (write_buf(data_buffer_, header_.len) == -1) {
                  buf_overrun_++;
                  printf("packer buf overrun\n");
                }
                reset();

              } else {
                crc_err_++;
                printf("crc_err 2:%ld\n", crc_err_);
                printf("header:%x,%x,%x\n", header_.preamble, header_.crc8,
                       header_.len);

                printf("calculated_crc8_:%x\n", calculated_crc8_);

                // for (uint32_t k = 0; k < header_.len; k++) {
                //   printf("%x ", data_buffer_[k]);
                // }
                // printf("\n");

                reset();
              }
            }
            // 如果数据还未收齐，保持STATE_PAYLOAD状态，等待下次数据
          }
        } break;

        default:
          reset();
          break;
      }
    }

    return 0;
  }

  // 传入数据进行解包
  int32_t putDataOneByte(const uint8_t* recv_buf, uint32_t received) {
    uint16_t packets_found = 0;

    for (uint16_t i = 0; i < received; i++) {
      if (processByte(recv_buf[i])) {
        packets_found++;
        // 将数据保存起来
        if (write_buf(data_buffer_, header_.len) == -1) {
          buf_overrun_++;
          printf("packer buf overrun\n");
        }
        reset();
      }
    }

    return packets_found;
  }

  // 批量处理数据
  int32_t processBuffer(const uint8_t* recv_buf, uint32_t received,
                        uint8_t* data, uint32_t length, uint32_t* data_len) {
    uint16_t packets_found = 0;

    for (uint16_t i = 0; i < received; i++) {
      bool is_unpack_success = false;

      switch (state_) {
        case STATE_PREAMBLE:
          if (recv_buf[i] == expected_preamble_) {
            state_ = STATE_CRC8;
            header_.preamble = recv_buf[i];
          }
          break;

        case STATE_CRC8:
          header_.crc8 = recv_buf[i];
          state_ = STATE_LENGTH_LOW;
          break;

        case STATE_LENGTH_LOW:
          header_.len = recv_buf[i];
          state_ = STATE_LENGTH_HIGH;
          break;

        case STATE_LENGTH_HIGH:
          header_.len |= (recv_buf[i] << 8);
          if (header_.len > MAX_PACKET_SIZE) {
            reset();
            break;
          }

          if (header_.len == 0) {
            // 没有数据载荷，直接reset
            reset();
          } else {
            state_ = STATE_PAYLOAD;
            data_buffer_cur_len_ = 0;
          }
          break;

        case STATE_PAYLOAD: {
          // 计算剩余需要的数据长度
          uint32_t remaining_data = header_.len - data_buffer_cur_len_;
          // 计算recv_buf中剩余的数据长度
          uint32_t remaining_buf = received - i;

          if (data_buffer_cur_len_ == 0 && remaining_buf >= remaining_data) {
            // 情况1：当前没有累积数据，且剩余数据足够整个包
            // 直接对recv_buf中的数据进行CRC校验
            calculated_crc8_ = calculateCRC8(recv_buf + i, header_.len);

            if (calculated_crc8_ == header_.crc8) {
              // CRC校验成功
              is_unpack_success = true;

              if (packets_found == 0) {
                // 第一个包，直接复制到输出data
                if (length < header_.len) {
                  return -1;
                }
                memcpy(data, recv_buf + i, header_.len);
                *data_len = header_.len;
              } else {
                // 第二个及以后的包，调用write_buf
                repeat_unpacke_++;
                if (write_buf(recv_buf + i, header_.len) == -1) {
                  buf_overrun_++;
                  printf("packer buf overrun\n");
                }
              }

              // 跳过已处理的数据
              i += header_.len - 1;  // -1 因为循环会i++
            } else {
              // CRC校验失败
              crc_err_++;
              printf("crc_err_:%ld\n", crc_err_);
              reset();
            }
          } else {
            // 情况2：有累积数据或剩余数据不足
            // 计算实际可以拷贝的数据量
            uint32_t copy_size = (remaining_buf < remaining_data)
                                     ? remaining_buf
                                     : remaining_data;

            // 将数据拷贝到data_buffer
            memcpy(data_buffer_ + data_buffer_cur_len_, recv_buf + i,
                   copy_size);
            data_buffer_cur_len_ += copy_size;

            // 跳过已拷贝的数据
            i += copy_size - 1;  // -1 因为循环会i++

            // 检查是否已经收到完整的数据包
            if (data_buffer_cur_len_ == header_.len) {
              // 数据已经收齐，进行CRC校验
              calculated_crc8_ = calculateCRC8(data_buffer_, header_.len);

              if (calculated_crc8_ == header_.crc8) {
                printf("data_buffer_ crc right\n");
                is_unpack_success = true;

                if (packets_found == 0) {
                  if (length < header_.len) {
                    return -1;
                  }
                  memcpy(data, data_buffer_, header_.len);
                  *data_len = header_.len;
                } else {
                  repeat_unpacke_++;
                  if (write_buf(data_buffer_, header_.len) == -1) {
                    buf_overrun_++;
                    printf("packer buf overrun\n");
                  }
                }
              } else {
                crc_err_++;
                printf("crc_err_:%ld\n", crc_err_);
                reset();
              }
            }
            // 如果数据还未收齐，保持STATE_PAYLOAD状态，等待下次数据
          }
        } break;

        default:
          reset();
          break;
      }

      if (is_unpack_success) {
        packets_found++;
        reset();
      }
    }

    return packets_found;  // 返回解析到的数据包数量
  }

  /*批量处理数据
  // 批量处理数据
  int32_t processBuffer(const uint8_t* recv_buf, uint32_t received,
                        uint8_t* data, uint32_t length, uint32_t* data_len) {
    uint16_t packets_found = 0;

    for (uint16_t i = 0; i < received; i++) {
      bool is_unpack_success = false;

      switch (state_) {
        case STATE_PREAMBLE_1:
          if (recv_buf[i] == expected_preamble_1_) {
            state_ = STATE_PREAMBLE_2;
            header_.preamble_1 = recv_buf[i];
          }
          break;

        case STATE_PREAMBLE_2:
          if (recv_buf[i] == expected_preamble_2_) {
            state_ = STATE_SEQ_ID;
            header_.preamble_2 = recv_buf[i];
          } else {
            reset();
          }
          break;

        case STATE_SEQ_ID:
          header_.seq_id = recv_buf[i];
          state_ = STATE_CRC8;
          break;

        case STATE_CRC8:
          header_.crc8 = recv_buf[i];
          state_ = STATE_LENGTH_LOW;
          break;

        case STATE_LENGTH_LOW:
          header_.len = recv_buf[i];
          state_ = STATE_LENGTH_HIGH;
          break;

        case STATE_LENGTH_HIGH:
          header_.len |= (recv_buf[i] << 8);
          if (header_.len > MAX_PACKET_SIZE) {
            reset();
            break;
          }

          state_ = STATE_RESERVE_LOW;
          break;

        case STATE_RESERVE_LOW:
          header_.reserve = recv_buf[i];
          state_ = STATE_RESERVE_HIGH;
          break;

        case STATE_RESERVE_HIGH:
          header_.reserve |= (recv_buf[i] << 8);

          if (header_.len == 0) {
            // 没有数据载荷，直接reset
            reset();
          } else {
            state_ = STATE_PAYLOAD;
            data_buffer_cur_len_ = 0;
          }
          break;

        case STATE_PAYLOAD: {
          // 计算剩余需要的数据长度
          uint32_t remaining_data = header_.len - data_buffer_cur_len_;
          // 计算recv_buf中剩余的数据长度
          uint32_t remaining_buf = received - i;

          if (data_buffer_cur_len_ == 0 && remaining_buf >= remaining_data) {
            // 情况1：当前没有累积数据，且剩余数据足够整个包
            // 直接对recv_buf中的数据进行CRC校验
            calculated_crc8_ = calculateCRC8(recv_buf + i, header_.len);

            if (calculated_crc8_ == header_.crc8) {
              // CRC校验成功
              // printf("crc right\n");
              // std::cout << "crc right" << std::endl;
              is_unpack_success = true;

              if (packets_found == 0) {
                // 第一个包，直接复制到输出data
                if (length < header_.len) {
                  return -1;
                }
                memcpy(data, recv_buf + i, header_.len);
                *data_len = header_.len;
              } else {
                // 第二个及以后的包，调用write_buf
                repeat_unpacke_++;
                if (write_buf(recv_buf + i, header_.len) == -1) {
                  buf_overrun_++;
                }
              }

              // 跳过已处理的数据
              i += header_.len - 1;  // -1 因为循环会i++
              // reset();
            } else {
              // CRC校验失败
              crc_err_++;
              printf("crc_err_:%ld\n", crc_err_);
              // printf("header:%d,%d,%d,%d,%d\n", header_.preamble_1,
              //        header_.preamble_2, header_.seq_id, header_.crc8,
              //        header_.len);
              // for (uint32_t j = 0; j < header_.len; j++) {
              //   printf("0x%02x,", recv_buf[i + j]);
              // }
              // printf("\n");

              // std::cout << "crc_err_:" << crc_err_ << std::endl;
              reset();
            }
          } else {
            // 情况2：有累积数据或剩余数据不足
            // 计算实际可以拷贝的数据量
            uint32_t copy_size = (remaining_buf < remaining_data)
                                     ? remaining_buf
                                     : remaining_data;

            // 将数据拷贝到data_buffer
            memcpy(data_buffer_ + data_buffer_cur_len_, recv_buf + i,
                   copy_size);
            data_buffer_cur_len_ += copy_size;

            // 跳过已拷贝的数据
            i += copy_size - 1;  // -1 因为循环会i++

            // 检查是否已经收到完整的数据包
            if (data_buffer_cur_len_ == header_.len) {
              // 数据已经收齐，进行CRC校验
              calculated_crc8_ = calculateCRC8(data_buffer_, header_.len);

              if (calculated_crc8_ == header_.crc8) {
                printf("data_buffer_ crc right\n");
                // std::cout << "data_buffer_ crc right" << std::endl;
                is_unpack_success = true;

                if (packets_found == 0) {
                  if (length < header_.len) {
                    return -1;
                  }
                  memcpy(data, data_buffer_, header_.len);
                  *data_len = header_.len;
                } else {
                  repeat_unpacke_++;
                  if (write_buf(data_buffer_, header_.len) == -1) {
                    buf_overrun_++;
                  }
                }
                // reset();
              } else {
                crc_err_++;
                printf("crc_err_:%ld\n", crc_err_);
                // printf("header:%d,%d,%d,%d,%d\n", header_.preamble_1,
                //        header_.preamble_2, header_.seq_id, header_.crc8,
                //        header_.len);
                // for (uint32_t j = 0; j < header_.len; j++) {
                //   printf("0x%02x,", recv_buf[i + j]);
                // }
                // printf("\n");

                // std::cout << "crc_err_:" << crc_err_ << std::endl;
                reset();
              }
            }
            // 如果数据还未收齐，保持STATE_PAYLOAD状态，等待下次数据
          }
        } break;

        default:
          reset();
          break;
      }

      if (is_unpack_success) {
        packets_found++;

        if (((header_.seq_id - last_seq_id_) == 1) ||
            (header_.seq_id == 0 && last_seq_id_ == 255)) {
          seq_id_right_++;
        } else {
          seq_id_err_++;
          printf("seq id err, seq:%d,last:%d\n", header_.seq_id,
          last_seq_id_);
          // printf("header:%d,%d,%d,%d,%d\n", header_.preamble_1,
          //        header_.preamble_2, header_.seq_id, header_.crc8,
          //        header_.len);
          // for (uint32_t j = 0; j < header_.len; j++) {
          //   printf("0x%02x,", recv_buf[i + j]);
          // }
          // printf("\n");
        }
        last_seq_id_ = header_.seq_id;
        reset();
      }
    }

    return packets_found;  // 返回解析到的数据包数量
  }
  */

  /*// 批量处理数据
  int32_t processBuffer(const uint8_t* recv_buf, uint32_t received,
                        uint8_t* data, uint32_t length, uint32_t* data_len) {
    uint16_t packets_found = 0;

    for (uint16_t i = 0; i < received; i++) {
      if (processByte(recv_buf[i])) {
        packets_found++;

        if (first_recv_seq_id) {
          last_seq_id_ = header_.seq_id;
          first_recv_seq_id = 0;
          seq_id_right_++;

        } else {
          cur_seq_id = header_.seq_id;
          if (((cur_seq_id == 0) && (last_seq_id_ == 255)) ||
              ((cur_seq_id - last_seq_id_) == 1)) {
            seq_id_right_++;
          } else {
            seq_id_err_++;
            // MyPrintf("seq_id err !!!cur:% d, last:%d\n", cur_seq_id,
            //          last_seq_id_);
          }
          last_seq_id_ = cur_seq_id;
        }

        if (length < header_.len) {
          return -1;
        }

        if (packets_found == 1) {
          memcpy(data, data_buffer_, header_.len);
          *data_len = header_.len;
        } else {
          repeat_unpacke_++;
          if (write_buf(data_buffer_, header_.len) == -1) {
            buf_overrun_++;
          }
        }
        reset();
      }
    }

    return packets_found;  // 成功解析，返回一包数据长度
  }
    */

  Header buildPacket2(const uint8_t* data, uint16_t data_length) {
    Header packer_header;

    // 设置前导字节
    packer_header.preamble = expected_preamble_;
    // 设置数据长度
    packer_header.len = data_length;
    // 计算数据段的CRC8
    packer_header.crc8 = calculateCRC8(data, data_length);

    return packer_header;
  }

  // 构建数据包
  uint16_t buildPacket(const uint8_t* data, uint16_t data_length,
                       uint8_t* packet_buffer, uint16_t buffer_size) {
    // 计算所需缓冲区大小
    uint16_t packet_size = sizeof(Header) + data_length;

    if (packet_size > buffer_size) {
      return 0;  // 缓冲区不足
    }

    // 填充Header
    Header* header_ptr = reinterpret_cast<Header*>(packet_buffer);
    header_ptr->preamble = expected_preamble_;
    header_ptr->len = data_length;

    // 计算数据段的CRC8
    header_ptr->crc8 = calculateCRC8(data, data_length);

    // 拷贝数据段
    if (data_length > 0 && data != nullptr) {
      memcpy(packet_buffer + sizeof(Header), data, data_length);
    }

    return packet_size;  // 返回实际数据包长度
  }

  // // 构建数据包（使用自定义保留字段）
  // uint16_t buildPacket(const uint8_t *data, uint16_t data_length,
  //                      uint8_t *packet_buffer, uint16_t buffer_size,
  //                      uint16_t reserve) {
  //   // 计算所需缓冲区大小
  //   uint16_t packet_size = sizeof(Header) + data_length;

  //   if (packet_size > buffer_size) {
  //     return 0; // 缓冲区不足
  //   }

  //   // 填充Header
  //   Header *header_ptr = reinterpret_cast<Header *>(packet_buffer);
  //   header_ptr->preamble_1 = expected_preamble_1_;
  //   header_ptr->preamble_2 = expected_preamble_2_;
  //   header_ptr->seq_id = send_seq_id_++;
  //   header_ptr->len = data_length;
  //   header_ptr->reserve = reserve; // 使用自定义保留字段

  //   // 计算数据段的CRC8
  //   header_ptr->crc8 = calculateCRC8(data, data_length);

  //   // 拷贝数据段
  //   if (data_length > 0 && data != nullptr) {
  //     memcpy(packet_buffer + sizeof(Header), data, data_length);
  //   }

  //   return packet_size; // 返回实际数据包长度
  // }

  // 读写buf操作
  //  判断缓冲区是否已满
  bool isBufferFull() {
    return ((packer_buf_end_ + 1) % PACKER_BUF_SIZE) == packer_buf_start_;
  }

  // 判断缓冲区是否为空
  bool isBufferEmpty() { return packer_buf_start_ == packer_buf_end_; }

  // 获取缓冲区中可用的数据包数量
  uint8_t getBufferCount() {
    if (packer_buf_end_ >= packer_buf_start_) {
      return packer_buf_end_ - packer_buf_start_;
    } else {
      return PACKER_BUF_SIZE - (packer_buf_start_ - packer_buf_end_);
    }
  }

  // 写入数据到缓冲区
  int32_t write_buf(const uint8_t* data, uint32_t len) {
    // 检查缓冲区是否有空位
    if (isBufferFull()) {
      return -1;  // 缓冲区已满
    }

    // 检查数据长度是否超过缓冲区限制
    if (len > PACKER_BUF_DATA_SIZE) {
      return -2;  // 数据过长
    }

    // 检查输入参数有效性
    if (data == nullptr && len > 0) {
      return -3;  // 数据指针无效
    }

    // 将数据和长度写入当前缓冲区位置
    packer_buf_[packer_buf_end_].len = len;
    if (len > 0 && data != nullptr) {
      memcpy(packer_buf_[packer_buf_end_].data, data, len);
    }

    // 更新缓冲区结束指针
    packer_buf_end_ = (packer_buf_end_ + 1) % PACKER_BUF_SIZE;

    return len;  // 返回实际写入的数据长度
  }

  // 从缓冲区读取数据
  int32_t read_buf(uint8_t* data, uint32_t len) {
    // 检查缓冲区是否有数据可读
    if (isBufferEmpty()) {
      return -1;  // 缓冲区为空
    }

    // 检查输出缓冲区是否足够大
    if (len < packer_buf_[packer_buf_start_].len) {
      return -2;  // 提供的缓冲区太小
    }

    // 检查输出参数有效性
    if (data == nullptr && packer_buf_[packer_buf_start_].len > 0) {
      return -3;  // 输出缓冲区指针无效
    }

    uint32_t data_len = packer_buf_[packer_buf_start_].len;

    // 将数据复制到输出缓冲区
    if (data_len > 0 && data != nullptr) {
      memcpy(data, packer_buf_[packer_buf_start_].data, data_len);
    }

    // 更新缓冲区开始指针
    packer_buf_start_ = (packer_buf_start_ + 1) % PACKER_BUF_SIZE;

    return data_len;  // 返回实际读取的数据长度
  }
};

#endif