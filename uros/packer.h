#include <cstdint>
#include <cstring>

struct __attribute__((aligned(4))) Header {
  uint8_t preamble_1;
  uint8_t preamble_2;
  uint8_t seq_id;
  uint8_t crc8;
  uint16_t len;
  uint16_t reserve;
};

struct PackerBuf {
  uint32_t len;
  uint8_t data[128];
};

class PacketParser {
private:
  enum ParserState {
    STATE_PREAMBLE_1,   // 寻找第一个前导字节
    STATE_PREAMBLE_2,   // 寻找第二个前导字节
    STATE_SEQ_ID,       // 序列号
    STATE_CRC8,         // CRC8校验码
    STATE_LENGTH_LOW,   // 长度低字节
    STATE_LENGTH_HIGH,  // 长度高字节
    STATE_RESERVE_LOW,  // 保留字段低字节
    STATE_RESERVE_HIGH, // 保留字段高字节
    STATE_PAYLOAD       // 读取数据载荷
  };

  // 内部缓冲区配置
  static const uint16_t MAX_PACKET_SIZE = 512; // 最大数据包大小
  static const uint16_t BUFFER_SIZE = MAX_PACKET_SIZE + sizeof(Header);

  ParserState state_;
  uint8_t expected_preamble_1; // 期望的前导字节1
  uint8_t expected_preamble_2; // 期望的前导字节2

public:
  uint16_t current_length; // 当前已接收数据长度
  // uint8_t received_crc8;    // 接收到的CRC8值
  uint8_t calculated_crc8; // 计算得到的CRC8值
  // uint8_t current_seq_id;   // 当前数据包的序列号
  uint32_t crc_err = 0;

  uint8_t cur_seq_id = 0;
  uint8_t last_seq_id = 0;
  bool first_recv_seq_id = 1;
  uint32_t seq_id_right = 0;
  uint32_t seq_id_err = 0;

  uint32_t repeat_unpacke = 0;
  uint32_t buf_overrun = 0;

  uint8_t data_buffer[BUFFER_SIZE]; // 内部数据包缓冲区
  uint16_t buffer_index;            // 缓冲区索引

  PackerBuf buf[4] = {0};
  uint8_t buf_start = 0;
  uint8_t buf_end = 0;

  Header header; // 包头

public:
  // 回调函数类型定义
  typedef void (*PacketCallback)(const Header *header, const uint8_t *data,
                                 uint16_t length);

private:
  PacketCallback packet_callback_;
  uint8_t send_seq_id_ = 0;

public:
  // 构造函数
  PacketParser(PacketCallback callback = nullptr, uint8_t preamble1 = 0xAA,
               uint8_t preamble2 = 0x55)
      : state_(STATE_PREAMBLE_1), expected_preamble_1(preamble1),
        expected_preamble_2(preamble2), current_length(0), calculated_crc8(0),
        buffer_index(0), packet_callback_(callback) {
    memset(&header, 0, sizeof(Header));
  }

  // 获取最大数据包大小
  uint16_t getMaxPacketSize() const { return MAX_PACKET_SIZE; }

  // 设置前导字节
  void setPreamble(uint8_t preamble1, uint8_t preamble2) {
    expected_preamble_1 = preamble1;
    expected_preamble_2 = preamble2;
  }

  // 重置解析器状态
  void reset() {
    state_ = STATE_PREAMBLE_1;
    current_length = 0;
    calculated_crc8 = 0;
    buffer_index = 0;
    memset(&header, 0, sizeof(Header));
  }

  // CRC8计算函数（只校验数据段）
  uint8_t calculateCRC8(const uint8_t *data, uint16_t length) {
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
    case STATE_PREAMBLE_1:
      if (byte == expected_preamble_1) {
        state_ = STATE_PREAMBLE_2;
        buffer_index = 0;
        // 存储第一个前导字节到header
        header.preamble_1 = byte;
      }
      break;

    case STATE_PREAMBLE_2:
      if (byte == expected_preamble_2) {
        state_ = STATE_SEQ_ID;
        header.preamble_2 = byte;
      } else {
        reset(); // 不是预期的前导字节，重置
      }
      break;

    case STATE_SEQ_ID:
      // current_seq_id = byte;
      header.seq_id = byte;
      state_ = STATE_CRC8;
      break;

    case STATE_CRC8:
      // received_crc8 = byte;
      header.crc8 = byte;
      state_ = STATE_LENGTH_LOW;
      break;

    case STATE_LENGTH_LOW:
      header.len = byte; // 先存储低字节
      state_ = STATE_LENGTH_HIGH;
      break;

    case STATE_LENGTH_HIGH:
      header.len |= (byte << 8);

      // 验证长度是否合理
      if (header.len > MAX_PACKET_SIZE) {
        reset(); // 长度无效，重置
        break;
      }

      state_ = STATE_RESERVE_LOW;
      break;

    case STATE_RESERVE_LOW:
      header.reserve = byte; // 先存储低字节
      state_ = STATE_RESERVE_HIGH;
      break;

    case STATE_RESERVE_HIGH:
      header.reserve |= (byte << 8); // 更新完整的保留字段

      if (header.len == 0) {
        // 没有数据载荷，直接进行CRC校验
        calculated_crc8 = calculateCRC8(nullptr, 0);
        if (calculated_crc8 == header.crc8) {
          // CRC校验成功
          if (packet_callback_) {
            packet_callback_(&header, nullptr, 0);
          }
          reset();
          return true;
        } else {
          reset(); // CRC校验失败
        }
      } else {
        state_ = STATE_PAYLOAD;
        current_length = 0;
      }
      break;

    case STATE_PAYLOAD:
      // 存储数据载荷
      if (buffer_index < BUFFER_SIZE) {
        data_buffer[buffer_index++] = byte;
        current_length++;

        // 检查是否接收完所有数据
        if (current_length >= header.len) {
          // 计算数据段的CRC8
          calculated_crc8 = calculateCRC8(data_buffer, header.len);

          if (calculated_crc8 == header.crc8) {
            // CRC校验成功
            // if (packet_callback_) {
            //   packet_callback_(&header, data_buffer, header.len);
            // }
            // reset();
            return true;
          } else {
            // CRC校验失败
            crc_err++;
            reset();
          }
        }
      } else {
        reset(); // 缓冲区溢出，重置
      }
      break;

    default:
      reset();
      break;
    }

    return false; // 数据包解析未完成或失败
  }

  // 批量处理数据
  int32_t processBuffer(const uint8_t *recv_buf, uint32_t received,
                        uint8_t *data, uint32_t length, uint32_t *data_len) {
    uint16_t packets_found = 0;

    for (uint16_t i = 0; i < received; i++) {
      bool is_unpack_success = 0;

      switch (state_) {
      case STATE_PREAMBLE_1:
        if (recv_buf[i] == expected_preamble_1) {
          state_ = STATE_PREAMBLE_2;
          buffer_index = 0;
          // 存储第一个前导字节到header
          header.preamble_1 = recv_buf[i];
        }
        break;

      case STATE_PREAMBLE_2:
        if (recv_buf[i] == expected_preamble_2) {
          state_ = STATE_SEQ_ID;
          header.preamble_2 = recv_buf[i];
        } else {
          reset(); // 不是预期的前导字节，重置
        }
        break;

      case STATE_SEQ_ID:
        // current_seq_id = byte;
        header.seq_id = recv_buf[i];
        state_ = STATE_CRC8;
        break;

      case STATE_CRC8:
        // received_crc8 = byte;
        header.crc8 = recv_buf[i];
        state_ = STATE_LENGTH_LOW;
        break;

      case STATE_LENGTH_LOW:
        header.len = recv_buf[i]; // 先存储低字节
        state_ = STATE_LENGTH_HIGH;
        break;

      case STATE_LENGTH_HIGH:
        header.len |= (recv_buf[i] << 8);

        // 验证长度是否合理
        if (header.len > MAX_PACKET_SIZE) {
          reset(); // 长度无效，重置
          break;
        }

        state_ = STATE_RESERVE_LOW;
        break;

      case STATE_RESERVE_LOW:
        header.reserve = recv_buf[i]; // 先存储低字节
        state_ = STATE_RESERVE_HIGH;
        break;

      case STATE_RESERVE_HIGH:
        header.reserve |= (recv_buf[i] << 8); // 更新完整的保留字段

        if (header.len == 0) {
          // 没有数据载荷，直接进行CRC校验
          calculated_crc8 = calculateCRC8(nullptr, 0);
          if (calculated_crc8 == header.crc8) {
            // CRC校验成功
            if (packet_callback_) {
              packet_callback_(&header, nullptr, 0);
            }
            reset();
            is_unpack_success = 1;
          } else {
            reset(); // CRC校验失败
          }
        } else {
          state_ = STATE_PAYLOAD;
          current_length = 0;
        }
        break;

      case STATE_PAYLOAD:
        // 存储数据载荷
        if (buffer_index < BUFFER_SIZE) {
          data_buffer[buffer_index++] = recv_buf[i];
          current_length++;

          // 检查是否接收完所有数据
          if (current_length >= header.len) {
            // 计算数据段的CRC8
            calculated_crc8 = calculateCRC8(data_buffer, header.len);

            if (calculated_crc8 == header.crc8) {
              // CRC校验成功
              // if (packet_callback_) {
              //   packet_callback_(&header, data_buffer, header.len);
              // }
              // reset();
              is_unpack_success = 1;
            } else {
              // CRC校验失败
              crc_err++;
              reset();
            }
          }
        } else {
          reset(); // 缓冲区溢出，重置
        }
        break;

      default:
        reset();
        break;
      }

      if (is_unpack_success) {
        is_unpack_success = 0;
        packets_found++;

        if (first_recv_seq_id) {
          last_seq_id = header.seq_id;
          first_recv_seq_id = 0;
          seq_id_right++;

        } else {
          cur_seq_id = header.seq_id;
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

        if (length < header.len) {
          return -1;
        }

        if (packets_found == 1) {
          memcpy(data, data_buffer, header.len);
          *data_len = header.len;
        } else {
          repeat_unpacke++;
          if (write_buf(data_buffer, header.len) == -1) {
            buf_overrun++;
          }
        }
        reset();
      }
    }

    return packets_found; // 成功解析，返回一包数据长度
  }

  // // 批量处理数据
  // int32_t processBuffer(const uint8_t *recv_buf, uint32_t received,
  //                       uint8_t *data, uint32_t length, uint32_t *data_len) {
  //   uint16_t packets_found = 0;

  //   for (uint16_t i = 0; i < received; i++) {
  //     if (processByte(recv_buf[i])) {
  //       packets_found++;

  //       if (first_recv_seq_id) {
  //         last_seq_id = header.seq_id;
  //         first_recv_seq_id = 0;
  //         seq_id_right++;

  //       } else {
  //         cur_seq_id = header.seq_id;
  //         if (((cur_seq_id == 0) && (last_seq_id == 255)) ||
  //             ((cur_seq_id - last_seq_id) == 1)) {
  //           seq_id_right++;
  //         } else {
  //           seq_id_err++;
  //           // MyPrintf("seq_id err !!!cur:% d, last:%d\n", cur_seq_id,
  //           //          last_seq_id);
  //         }
  //         last_seq_id = cur_seq_id;
  //       }

  //       if (length < header.len) {
  //         return -1;
  //       }

  //       if (packets_found == 1) {
  //         memcpy(data, data_buffer, header.len);
  //         *data_len = header.len;
  //       } else {
  //         repeat_unpacke++;
  //         if (write_buf(data_buffer, header.len) == -1) {
  //           buf_overrun++;
  //         }
  //       }
  //       reset();
  //     }
  //   }

  //   return packets_found; // 成功解析，返回一包数据长度
  // }

  Header buildPacket2(const uint8_t *data, uint16_t data_length) {
    Header packer_header;

    // 设置前导字节（使用默认值或类成员变量）
    packer_header.preamble_1 = expected_preamble_1; // 使用类中的前导字节配置
    packer_header.preamble_2 = expected_preamble_2;
    // 设置序列号并递增
    packer_header.seq_id = send_seq_id_++;
    // 设置数据长度
    packer_header.len = data_length;
    // 设置保留字段（默认为0）
    packer_header.reserve = 0;
    // 计算数据段的CRC8
    packer_header.crc8 = calculateCRC8(data, data_length);

    return packer_header;
  }

  // 构建数据包
  uint16_t buildPacket(const uint8_t *data, uint16_t data_length,
                       uint8_t *packet_buffer, uint16_t buffer_size) {
    // 计算所需缓冲区大小
    uint16_t packet_size = sizeof(Header) + data_length;

    if (packet_size > buffer_size) {
      return 0; // 缓冲区不足
    }

    // 填充Header
    Header *header_ptr = reinterpret_cast<Header *>(packet_buffer);
    header_ptr->preamble_1 = expected_preamble_1;
    header_ptr->preamble_2 = expected_preamble_2;
    header_ptr->seq_id = send_seq_id_++;
    header_ptr->len = data_length;
    header_ptr->reserve = 0; // 保留字段设为0

    // 计算数据段的CRC8
    header_ptr->crc8 = calculateCRC8(data, data_length);

    // 拷贝数据段
    if (data_length > 0 && data != nullptr) {
      memcpy(packet_buffer + sizeof(Header), data, data_length);
    }

    return packet_size; // 返回实际数据包长度
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
  //   header_ptr->preamble_1 = expected_preamble_1;
  //   header_ptr->preamble_2 = expected_preamble_2;
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

  //读写buf操作
  // 判断缓冲区是否已满
  bool isBufferFull() { return ((buf_end + 1) % 4) == buf_start; }

  // 判断缓冲区是否为空
  bool isBufferEmpty() { return buf_start == buf_end; }

  // 获取缓冲区中可用的数据包数量
  uint8_t getBufferCount() {
    if (buf_end >= buf_start) {
      return buf_end - buf_start;
    } else {
      return 4 - (buf_start - buf_end);
    }
  }

  // 写入数据到缓冲区
  int32_t write_buf(uint8_t *data, uint32_t len) {
    // 检查缓冲区是否有空位
    if (isBufferFull()) {
      return -1; // 缓冲区已满
    }

    // 检查数据长度是否超过缓冲区限制
    if (len > 128) {
      return -2; // 数据过长
    }

    // 检查输入参数有效性
    if (data == nullptr && len > 0) {
      return -3; // 数据指针无效
    }

    // 将数据和长度写入当前缓冲区位置
    buf[buf_end].len = len;
    if (len > 0 && data != nullptr) {
      memcpy(buf[buf_end].data, data, len);
    }

    // 更新缓冲区结束指针
    buf_end = (buf_end + 1) % 4;

    return len; // 返回实际写入的数据长度
  }

  // 从缓冲区读取数据
  int32_t read_buf(uint8_t *data, uint32_t len) {
    // 检查缓冲区是否有数据可读
    if (isBufferEmpty()) {
      return -1; // 缓冲区为空
    }

    // 检查输出缓冲区是否足够大
    if (len < buf[buf_start].len) {
      return -2; // 提供的缓冲区太小
    }

    // 检查输出参数有效性
    if (data == nullptr && buf[buf_start].len > 0) {
      return -3; // 输出缓冲区指针无效
    }

    uint32_t data_len = buf[buf_start].len;

    // 将数据复制到输出缓冲区
    if (data_len > 0 && data != nullptr) {
      memcpy(data, buf[buf_start].data, data_len);
    }

    // 更新缓冲区开始指针
    buf_start = (buf_start + 1) % 4;

    return data_len; // 返回实际读取的数据长度
  }
};