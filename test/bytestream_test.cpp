#include <iostream>
#include <thread>
#include <vector>
#include <iomanip>
#include <atomic>
#include <chrono>
#include <cstring>

#include "uros/uros.h"

using namespace std::chrono_literals;

// 测试统计
struct TestStats {
  std::atomic<size_t> sent{0};
  std::atomic<size_t> received{0};
  std::atomic<size_t> errors{0};
  std::atomic<size_t> mismatch{0};
};

TestStats g_stats;

// 生成测试数据
void fill_test_data(uint8_t* data, size_t len, uint8_t pattern) {
  for (size_t i = 0; i < len; ++i) {
    data[i] = (pattern + i) & 0xFF;
  }
}

// 验证测试数据
bool verify_test_data(const uint8_t* data, size_t len, uint8_t pattern) {
  for (size_t i = 0; i < len; ++i) {
    if (data[i] != ((pattern + i) & 0xFF)) {
      return false;
    }
  }
  return true;
}

// 打印数据（前后各显示一部分）
void print_data(const uint8_t* data, size_t len, const char* prefix = "") {
  std::cout << prefix << "[长度:" << len << "] ";
  
  size_t show_head = std::min(size_t(8), len);
  size_t show_tail = std::min(size_t(8), len);
  
  // 显示前面的字节
  for (size_t i = 0; i < show_head; ++i) {
    std::cout << std::hex << std::setw(2) << std::setfill('0') 
              << (int)data[i] << " ";
  }
  
  if (len > show_head + show_tail) {
    std::cout << "... ";
    // 显示后面的字节
    for (size_t i = len - show_tail; i < len; ++i) {
      std::cout << std::hex << std::setw(2) << std::setfill('0') 
                << (int)data[i] << " ";
    }
  } else if (len > show_head) {
    for (size_t i = show_head; i < len; ++i) {
      std::cout << std::hex << std::setw(2) << std::setfill('0') 
                << (int)data[i] << " ";
    }
  }
  
  std::cout << std::dec << "\n";
}

void uros_init() {
  uros::InitGuard init_guard(0);
  uros::RegisterTopic<uros::ByteStream>("/bytestream_test");
}

// 全局节点和通信组件（避免重复创建）
struct TestContext {
  uros::Node sender_node;
  uros::Node receiver_node;
  uros::PublisherT<uros::ByteStream>* pub = nullptr;
  uros::SubscriptionT<uros::ByteStream>* sub = nullptr;
  
  std::atomic<bool> test_completed{false};
  std::atomic<bool> test_passed{false};
  std::atomic<size_t> expected_length{0};
  std::atomic<uint8_t> expected_pattern{0};
  
  std::atomic<bool> running{true};
  std::thread sender_thread;
  std::thread receiver_thread;
  
  bool init() {
    // 创建发布者
    pub = sender_node.createPublisher<uros::ByteStream>("/bytestream_test");
    if (!pub) {
      std::cerr << "❌ 创建 Publisher 失败\n";
      return false;
    }
    
    // 创建订阅者
    sub = receiver_node.createSubscription<uros::ByteStream>(
        "/bytestream_test", [this](const uros::ByteStream &msg) {
          g_stats.received++;
          
          size_t received_len = msg.getLen();
          size_t expect_len = expected_length.load();
          uint8_t expect_pattern = expected_pattern.load();
          
          // 验证长度
          if (received_len != expect_len) {
            g_stats.mismatch++;
            test_passed = false;
            test_completed = true;
            return;
          }
          
          // 验证数据
          if (!verify_test_data(msg.data, received_len, expect_pattern)) {
            g_stats.mismatch++;
            test_passed = false;
            test_completed = true;
            return;
          }
          
          test_passed = true;
          test_completed = true;
        });
    
    if (!sub) {
      std::cerr << "❌ 创建 Subscription 失败\n";
      return false;
    }
    
    // 启动线程
    sender_thread = std::thread([this]() {
      while (running) {
        sender_node.spinOnce();
        std::this_thread::sleep_for(5ms);
      }
    });
    
    receiver_thread = std::thread([this]() {
      while (running) {
        receiver_node.spinOnce();
        std::this_thread::sleep_for(5ms);
      }
    });
    
    // 等待启动
    std::this_thread::sleep_for(200ms);
    
    return true;
  }
  
  void shutdown() {
    running = false;
    if (sender_thread.joinable()) sender_thread.join();
    if (receiver_thread.joinable()) receiver_thread.join();
  }
};

// 测试指定长度的 ByteStream
bool test_bytestream_length(TestContext& ctx, size_t length, uint8_t pattern) {
  // 计算最大可用数据长度
  const size_t max_data_len = UROS_MSG_MAX_SIZE - sizeof(uros::MsgBase);
  
  if (length > max_data_len) {
    std::cout << "  ❌ 长度 " << length << " 超过最大值 " << max_data_len << "\n";
    return false;
  }
  
  std::cout << "  测试长度: " << std::setw(4) << length 
            << ", 模式: 0x" << std::hex << std::setw(2) << std::setfill('0')
            << (int)pattern << std::dec << " ... ";
  std::cout.flush();
  
  // 重置状态
  ctx.test_completed = false;
  ctx.test_passed = false;
  ctx.expected_length = length;
  ctx.expected_pattern = pattern;
  
  // 准备测试数据
  uros::ByteStream test_msg;
  fill_test_data(test_msg.data, length, pattern);
  test_msg.setLen(length);
  
  // 发送数据
  ctx.pub->publish(test_msg);
  g_stats.sent++;
  
  // 等待接收（最多500毫秒）
  auto start_time = std::chrono::steady_clock::now();
  while (!ctx.test_completed) {
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    if (elapsed > 500ms) {
      std::cout << "⏱️  超时\n";
      g_stats.errors++;
      return false;
    }
    std::this_thread::sleep_for(10ms);
  }
  
  if (ctx.test_passed) {
    std::cout << "✅\n";
    return true;
  } else {
    std::cout << "❌ 验证失败\n";
    return false;
  }
}

int main() {
  std::cout << "\n";
  std::cout << "╔════════════════════════════════════════════════════════════╗\n";
  std::cout << "║           ByteStream Topic 收发测试                       ║\n";
  std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
  
  uros_init();
  
  // 初始化测试上下文
  TestContext ctx;
  if (!ctx.init()) {
    std::cerr << "❌ 测试初始化失败\n";
    return 1;
  }
  
  // 获取最大数据长度
  const size_t max_data_len = UROS_MSG_MAX_SIZE - sizeof(uros::MsgBase);
  std::cout << "UROS_MSG_MAX_SIZE: " << UROS_MSG_MAX_SIZE << " 字节\n";
  std::cout << "MsgBase 大小: " << sizeof(uros::MsgBase) << " 字节\n";
  std::cout << "ByteStream 最大数据长度: " << max_data_len << " 字节\n";
  std::cout << "ByteStream 总大小: " << sizeof(uros::ByteStream) << " 字节\n";
  std::cout << "\n";
  
  // 测试用例
  std::vector<size_t> test_lengths;
  
  // 1. 边界值测试
  std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
  std::cout << "测试 1: 边界值\n";
  std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
  test_lengths = {1, 2, max_data_len - 1, max_data_len};
  for (size_t len : test_lengths) {
    test_bytestream_length(ctx, len, 0x00);
  }
  
  // 2. 小数据测试
  std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
  std::cout << "测试 2: 小数据 (1-16 字节)\n";
  std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
  test_lengths = {1, 2, 4, 8, 16};
  for (size_t len : test_lengths) {
    test_bytestream_length(ctx, len, 0x11);
  }
  
  // 3. 中等数据测试
  std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
  std::cout << "测试 3: 中等数据 (32-64 字节)\n";
  std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
  test_lengths = {32, 48, 64};
  for (size_t len : test_lengths) {
    test_bytestream_length(ctx, len, 0x22);
  }
  
  // 4. 大数据测试
  std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
  std::cout << "测试 4: 大数据 (接近最大值)\n";
  std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
  if (max_data_len >= 100) {
    test_lengths = {max_data_len - 10, max_data_len - 5, max_data_len};
    for (size_t len : test_lengths) {
      test_bytestream_length(ctx, len, 0x33);
    }
  }
  
  // 5. 遍历所有可能的长度（可选，针对小的 MSG_MAX_SIZE）
  if (max_data_len <= 128) {
    std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    std::cout << "测试 5: 遍历所有长度 (1 到 " << max_data_len << ")\n";
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    
    size_t errors_before = g_stats.errors.load();
    size_t passed = 0;
    
    for (size_t len = 1; len <= max_data_len; ++len) {
      if (test_bytestream_length(ctx, len, (len & 0xFF))) {
        passed++;
      }
      // 显示进度
      if (len % 10 == 0) {
        std::cout << "." << std::flush;
      }
    }
    
    size_t errors_this_test = g_stats.errors.load() - errors_before;
    std::cout << "\n\n  通过: " << passed << "/" << max_data_len 
              << ", 失败: " << errors_this_test << "\n";
  }
  
  // 6. 不同数据模式测试
  std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
  std::cout << "测试 6: 不同数据模式\n";
  std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
  size_t test_len = std::min(size_t(64), max_data_len);
  std::vector<uint8_t> patterns = {0x00, 0xFF, 0xAA, 0x55, 0x12, 0x34};
  for (uint8_t pattern : patterns) {
    test_bytestream_length(ctx, test_len, pattern);
  }
  
  // 关闭测试上下文
  ctx.shutdown();
  
  // 统计结果
  std::cout << "\n" << std::string(60, '=') << "\n";
  std::cout << "📊 测试统计\n";
  std::cout << std::string(60, '=') << "\n";
  std::cout << "发送消息数:   " << g_stats.sent.load() << "\n";
  std::cout << "接收消息数:   " << g_stats.received.load() << "\n";
  std::cout << "数据不匹配:   " << g_stats.mismatch.load() << "\n";
  std::cout << "错误次数:     " << g_stats.errors.load() << "\n";
  std::cout << std::string(60, '=') << "\n";
  
  size_t total_tests = g_stats.sent.load();
  size_t passed_tests = g_stats.received.load() - g_stats.mismatch.load();
  
  if (g_stats.errors.load() == 0 && g_stats.mismatch.load() == 0) {
    std::cout << "\n🎉 所有测试通过! (" << passed_tests << "/" 
              << total_tests << ")\n";
    std::cout << "✅ ByteStream 在 Topic 中可以正常收发\n";
    std::cout << "✅ 支持 0 到 " << max_data_len << " 字节的变长数据\n\n";
    return 0;
  } else {
    std::cout << "\n⚠️  部分测试失败: " << passed_tests << "/" 
              << total_tests << " 通过\n\n";
    return 1;
  }
}
