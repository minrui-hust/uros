#include <cstdint>
#include <cstring>

#include "etl/vector.h"

struct PackerBuf {
  uint32_t len;
  uint8_t data[128];
};

struct __attribute__((aligned(4))) Header {
  uint8_t preamble_1;
  uint8_t preamble_2;
  uint8_t seq_id;
};

// #pragma pack(push, 4)
// struct header {
//   uint8_t preamble_1;
//   uint8_t preamble_2;
//   uint8_t seq_id;
// }
// #pragma pack(pop)

// 数据包定义
class PacketParser {
private:
  enum ParserState {
    STATE_PREAMBLE_1,  // 寻找第一个'$'
    STATE_PREAMBLE_2,  // 寻找第二个'$'
    STATE_SEQ_ID,      // id
    STATE_LENGTH_HIGH, // 读取长度高字节
    STATE_LENGTH_LOW,  // 读取长度低字节
    STATE_PAYLOAD,     // 读取数据载荷
    STATE_CRC_1,       // CRC字节1
    STATE_CRC_2,       // CRC字节2
    STATE_CRC_3,       // CRC字节3
    STATE_CRC_4,       // CRC字节4
    STATE_END_1,       // 寻找第一个'#'
    STATE_END_2        // 寻找第二个'#'
  };

  // 内部缓冲区配置
  static const uint16_t MAX_PACKET_SIZE = 512; // 最大数据包大小
  static const uint16_t BUFFER_SIZE =
      MAX_PACKET_SIZE + 10; // 缓冲区大小（包含包头包尾）

  ParserState state_;

public:
  uint16_t expected_length; // 期望的数据长度
  uint16_t current_length;  // 当前已接收数据长度
  uint32_t received_crc;    // 接收到的CRC值
  uint32_t calculated_crc;  // 计算得到的CRC值
  uint8_t current_seq_id;   // 当前数据包的序列号

  uint8_t data_buffer[BUFFER_SIZE]; // 内部数据包缓冲区
  uint16_t buffer_index;            // 缓冲区索引

  PackerBuf buf[4] = {0};
  uint8_t buf_start = 0;
  uint8_t buf_end = 0;

public:
  // 回调函数类型定义
  typedef void (*PacketCallback)(const uint8_t *data, uint16_t length);

private:
  PacketCallback packet_callback_;
  uint32_t send_seq_id_ = 0;

public:
  // 构造函数简化，无需外部传入缓冲区
  PacketParser(PacketCallback callback = nullptr)
      : state_(STATE_PREAMBLE_1), expected_length(0), current_length(0),
        received_crc(0), calculated_crc(0), current_seq_id(0), buffer_index(0),
        packet_callback_(callback) {}

  // 获取最大数据包大小
  uint16_t getMaxPacketSize() const { return MAX_PACKET_SIZE; }

  // 重置解析器状态
  void reset() {
    state_ = STATE_PREAMBLE_1;
    expected_length = 0;
    current_length = 0;
    received_crc = 0;
    calculated_crc = 0;
    buffer_index = 0;
  }

  // CRC32计算函数
  uint32_t calculateCRC32(const uint8_t *data, uint16_t length) {
    // 简单的CRC32实现
    uint32_t crc = 0xFFFFFFFF;
    for (uint16_t i = 0; i < length; i++) {
      crc ^= data[i];
      for (int j = 0; j < 8; j++) {
        if (crc & 1) {
          crc = (crc >> 1) ^ 0xEDB88320;
        } else {
          crc = crc >> 1;
        }
      }
    }
    return ~crc;
  }

  // 处理接收到的字节
  bool processByte(uint8_t byte) {
    switch (state_) {
    case STATE_PREAMBLE_1:
      if (byte == '$') {
        state_ = STATE_PREAMBLE_2;
        buffer_index = 0;
      }
      break;

    case STATE_PREAMBLE_2:
      if (byte == '$') {
        state_ = STATE_SEQ_ID;
      } else {
        reset(); // 不是预期的'$'，重置
      }
      break;

    case STATE_SEQ_ID:       // 新增：读取序列号
      current_seq_id = byte; // 存储序列号
      state_ = STATE_LENGTH_HIGH;
      break;

    case STATE_LENGTH_HIGH:
      expected_length = byte; // 长度低字节
      state_ = STATE_LENGTH_LOW;
      break;

    case STATE_LENGTH_LOW:
      expected_length |= byte << 8; // 长度高字节

      // 验证长度是否合理
      if (expected_length == 0 || expected_length > MAX_PACKET_SIZE) {
        reset(); // 长度无效，重置
        break;
      }

      state_ = STATE_PAYLOAD;
      current_length = 0;
      break;

    case STATE_PAYLOAD:
      // 存储数据载荷
      if (buffer_index < BUFFER_SIZE) {
        data_buffer[buffer_index++] = byte;
        current_length++;

        // 检查是否接收完所有数据
        if (current_length >= expected_length) {
          state_ = STATE_CRC_1;
          received_crc = 0;
        }
      } else {
        reset(); // 缓冲区溢出，重置
      }
      break;

    case STATE_CRC_1:
      received_crc = byte;
      state_ = STATE_CRC_2;
      break;

    case STATE_CRC_2:
      received_crc |= byte << 8;
      state_ = STATE_CRC_3;
      break;

    case STATE_CRC_3:
      received_crc |= byte << 16;
      state_ = STATE_CRC_4;
      break;

    case STATE_CRC_4:
      received_crc |= byte << 24;
      state_ = STATE_END_1;
      break;

    case STATE_END_1:
      if (byte == '#') {
        state_ = STATE_END_2;
      } else {
        reset(); // 不是预期的'#'，重置
      }
      break;

    case STATE_END_2:
      if (byte == '#') {
        // 完整的包接收完成，进行CRC校验

        // 计算CRC（对长度字段和数据载荷进行校验）
        uint8_t crc_buffer[2 + expected_length];
        crc_buffer[0] = expected_length & 0xFF;               // 长度低字节
        crc_buffer[1] = (expected_length >> 8) & 0xFF;        // 长度高字节
        memcpy(crc_buffer + 2, data_buffer, expected_length); // 数据载荷

        calculated_crc = calculateCRC32(crc_buffer, 2 + expected_length);

        if (calculated_crc == received_crc) {

          // CRC校验通过
          //   if (packet_callback_) {
          //     packet_callback_(data_buffer, expected_length);
          //   }
          //   reset();
          return true; // 成功解析一个完整的数据包
        } else {
          // CRC校验失败
          //   std::cout << "crc error': " << std::endl;
          reset();
        }
      } else {
        reset(); // 不是预期的'#'，重置
      }
      break;

    default:
      reset();
      break;
    }

    return false; // 数据包解析未完成或失败
  }

  // 批量处理数据
  uint16_t processBuffer(const uint8_t *data, uint16_t length) {
    uint16_t packets_found = 0;

    for (uint16_t i = 0; i < length; i++) {
      if (processByte(data[i])) {
        packets_found++; // 成功解析一个完整数据包
      }
    }

    return packets_found; // 返回解析到的数据包数量
  }

  uint32_t buildPacket(const uint8_t *data, uint16_t data_length,
                       uint8_t *packet_buffer, uint16_t buffer_size) {
    // 计算所需缓冲区大小
    uint16_t packet_size =
        2 + 1 + 2 + data_length + 4 + 2; // $$ + 长度 + 数据 + CRC + ##

    if (packet_size > buffer_size) {
      return 0; // 缓冲区不足
    }

    uint16_t index = 0;

    // 1. 前导码：$$（固定字节，无字节序问题）
    packet_buffer[index++] = '$';
    packet_buffer[index++] = '$';

    //  序列号（1字节）
    packet_buffer[index++] = send_seq_id_++;

    // 2. 数据长度（2字节，STM32小端 - 直接存储）
    // 在STM32小端系统中，直接memcpy即可
    memcpy(&packet_buffer[index], &data_length, sizeof(data_length));
    index += sizeof(data_length);

    // 3. 数据载荷
    if (data_length > 0 && data != nullptr) {
      memcpy(&packet_buffer[index], data, data_length);
      index += data_length;
    }

    // 4. CRC32校验（4字节，STM32小端 - 直接存储）
    // 准备CRC计算的数据（长度字段 + 数据载荷）
    uint8_t crc_data[2 + data_length];
    memcpy(crc_data, &data_length, sizeof(data_length)); //直接拷贝长度（小端）
    if (data_length > 0 && data != nullptr) {
      memcpy(&crc_data[2], data, data_length);
    }

    uint32_t crc_value = calculateCRC32(crc_data, 2 + data_length);

    // STM32小端，直接memcpy CRC值
    memcpy(&packet_buffer[index], &crc_value, sizeof(crc_value));
    index += sizeof(crc_value);

    // 5. 结束符：##（固定字节，无字节序问题）
    packet_buffer[index++] = '#';
    packet_buffer[index++] = '#';

    return index; // 返回实际数据包长度
  }

  // 设置数据包回调函数
  void setPacketCallback(PacketCallback callback) {
    packet_callback_ = callback;
  }

  // 获取当前解析状态（用于调试）
  ParserState getState() const { return state_; }

  // 获取当前期望的数据长度（用于调试）
  uint16_t getExpectedLength() const { return expected_length; }

  // 获取当前已接收的数据长度（用于调试）
  uint16_t getCurrentLength() const { return current_length; }

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