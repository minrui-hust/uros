#include <cstdint>
#include <cstring>

// 数据包定义
class PacketParser {
private:
  enum ParserState {
    STATE_PREAMBLE_1,  // 寻找第一个'$'
    STATE_PREAMBLE_2,  // 寻找第二个'$'
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
  uint16_t expected_length_; // 期望的数据长度
  uint16_t current_length_;  // 当前已接收数据长度
  uint32_t received_crc_;    // 接收到的CRC值
  uint32_t calculated_crc_;  // 计算得到的CRC值

  uint8_t packet_buffer_[BUFFER_SIZE]; // 内部数据包缓冲区
  uint16_t buffer_index_;              // 缓冲区索引

public:
  // 回调函数类型定义
  typedef void (*PacketCallback)(const uint8_t *data, uint16_t length);

private:
  PacketCallback packet_callback_;

public:
  // 构造函数简化，无需外部传入缓冲区
  PacketParser(PacketCallback callback = nullptr)
      : state_(STATE_PREAMBLE_1), expected_length_(0), current_length_(0),
        received_crc_(0), calculated_crc_(0), buffer_index_(0),
        packet_callback_(callback) {}

  // 获取最大数据包大小
  uint16_t getMaxPacketSize() const { return MAX_PACKET_SIZE; }

  // 重置解析器状态
  void reset() {
    state_ = STATE_PREAMBLE_1;
    expected_length_ = 0;
    current_length_ = 0;
    received_crc_ = 0;
    calculated_crc_ = 0;
    buffer_index_ = 0;
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
        buffer_index_ = 0;
      }
      break;

    case STATE_PREAMBLE_2:
      if (byte == '$') {
        state_ = STATE_LENGTH_HIGH;
      } else {
        reset(); // 不是预期的'$'，重置
      }
      break;

    case STATE_LENGTH_HIGH:
      expected_length_ = byte; // 长度低字节
      state_ = STATE_LENGTH_LOW;
      break;

    case STATE_LENGTH_LOW:
      expected_length_ |= byte << 8; // 长度高字节

      // 验证长度是否合理
      if (expected_length_ == 0 || expected_length_ > MAX_PACKET_SIZE) {
        reset(); // 长度无效，重置
        break;
      }

      state_ = STATE_PAYLOAD;
      current_length_ = 0;
      break;

    case STATE_PAYLOAD:
      // 存储数据载荷
      if (buffer_index_ < BUFFER_SIZE) {
        packet_buffer_[buffer_index_++] = byte;
        current_length_++;

        // 检查是否接收完所有数据
        if (current_length_ >= expected_length_) {
          state_ = STATE_CRC_1;
          received_crc_ = 0;
        }
      } else {
        reset(); // 缓冲区溢出，重置
      }
      break;

    case STATE_CRC_1:
      received_crc_ = byte;
      state_ = STATE_CRC_2;
      break;

    case STATE_CRC_2:
      received_crc_ |= byte << 8;
      state_ = STATE_CRC_3;
      break;

    case STATE_CRC_3:
      received_crc_ |= byte << 16;
      state_ = STATE_CRC_4;
      break;

    case STATE_CRC_4:
      received_crc_ |= byte << 24;
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
        uint8_t crc_buffer[2 + expected_length_];
        crc_buffer[0] = expected_length_ & 0xFF;        // 长度低字节
        crc_buffer[1] = (expected_length_ >> 8) & 0xFF; // 长度高字节
        memcpy(crc_buffer + 2, packet_buffer_, expected_length_); // 数据载荷

        calculated_crc_ = calculateCRC32(crc_buffer, 2 + expected_length_);

        if (calculated_crc_ == received_crc_) {

          // CRC校验通过
          //   if (packet_callback_) {
          //     packet_callback_(packet_buffer_, expected_length_);
          //   }
          //   reset();
          return true; // 成功解析一个完整的数据包
        } else {
          // CRC校验失败
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
        2 + 2 + data_length + 4 + 2; // $$ + 长度 + 数据 + CRC + ##

    if (packet_size > buffer_size) {
      return 0; // 缓冲区不足
    }

    uint16_t index = 0;

    // 1. 前导码：$$（固定字节，无字节序问题）
    packet_buffer[index++] = '$';
    packet_buffer[index++] = '$';

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
    memcpy(crc_data, &data_length, sizeof(data_length)); // 直接拷贝长度（小端）
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
  uint16_t getExpectedLength() const { return expected_length_; }

  // 获取当前已接收的数据长度（用于调试）
  uint16_t getCurrentLength() const { return current_length_; }
};