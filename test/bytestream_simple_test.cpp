#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <iomanip>

#include "uros/uros.h"

using namespace std::chrono_literals;

// 简单的测试统计
std::atomic<int> sent_count{0};
std::atomic<int> received_count{0};
std::atomic<int> error_count{0};

// 生成测试数据
void fill_data(uint8_t* data, size_t len, uint8_t pattern) {
  for (size_t i = 0; i < len; ++i) {
    data[i] = (pattern + i) & 0xFF;
  }
}

// 验证数据
bool verify_data(const uint8_t* data, size_t len, uint8_t pattern) {
  for (size_t i = 0; i < len; ++i) {
    if (data[i] != ((pattern + i) & 0xFF)) {
      return false;
    }
  }
  return true;
}

void uros_init() {
  uros::InitGuard init_guard(0);
  uros::RegisterTopic<uros::ByteStream>("/bytestream_test");
}

int main() {
  std::cout << "\n╔═══════════════════════════════════════════════╗\n";
  std::cout << "║    ByteStream Topic 变长测试               ║\n";
  std::cout << "╚═══════════════════════════════════════════════╝\n\n";
  
  uros_init();
  
  const size_t max_len = UROS_MSG_MAX_SIZE - sizeof(uros::MsgBase);
  std::cout << "最大数据长度: " << max_len << " 字节\n\n";
  
  // 创建节点
  uros::Node sender_node;
  uros::Node receiver_node;
  
  // 创建发布者
  auto pub = sender_node.createPublisher<uros::ByteStream>("/bytestream_test");
  if (!pub) {
    std::cerr << "❌ 创建发布者失败\n";
    return 1;
  }
  
  // 接收到的最后一条消息
  std::atomic<size_t> last_received_len{0};
  std::atomic<uint8_t> last_received_pattern{0};
  std::atomic<bool> message_received{false};
  
  // 创建订阅者
  auto sub = receiver_node.createSubscription<uros::ByteStream>(
      "/bytestream_test", [&](const uros::ByteStream &msg) {
        received_count++;
        size_t len = msg.getLen();
        last_received_len = len;
        
        // 提取pattern (第一个字节)
        uint8_t pattern = (len > 0) ? msg.data[0] : 0;
        last_received_pattern = pattern;
        
        // 验证数据
        if (!verify_data(msg.data, len, pattern)) {
          std::cout << "    ❌ 数据验证失败 (长度:" << len << ")\n";
          error_count++;
        }
        
        message_received = true;
      });
  
  if (!sub) {
    std::cerr << "❌ 创建订阅者失败\n";
    return 1;
  }
  
  // 启动接收线程
  std::atomic<bool> running{true};
  std::thread receiver_thread([&]() {
    while (running) {
      receiver_node.spinOnce();
      std::this_thread::sleep_for(10ms);
    }
  });
  
  std::thread sender_thread([&]() {
    while (running) {
      sender_node.spinOnce();
      std::this_thread::sleep_for(10ms);
    }
  });
  
  // 等待节点启动
  std::this_thread::sleep_for(200ms);
  
  std::cout << "开始测试不同长度...\n\n";
  
  // 测试不同长度
  std::vector<size_t> test_lengths = {
    1, 2, 4, 8, 16, 32, 64, 96, 120  // max_len should be 120
  };
  
  for (size_t len : test_lengths) {
    if (len > max_len) {
      std::cout << "  跳过长度 " << len << " (超过最大值)\n";
      continue;
    }
    
    uint8_t pattern = (len & 0xFF);
    
    // 准备消息
    uros::ByteStream msg;
    fill_data(msg.data, len, pattern);
    msg.setLen(len);
    
    // 重置接收标志
    message_received = false;
    
    // 发送
    pub->publish(msg);
    sent_count++;
    
    std::cout << "  长度 " << std::setw(3) << len 
              << " (pattern:0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)pattern << std::dec << ") ... " << std::flush;
    
    // 等待接收 (最多1秒)
    auto start = std::chrono::steady_clock::now();
    while (!message_received && 
           (std::chrono::steady_clock::now() - start) < 1s) {
      std::this_thread::sleep_for(10ms);
    }
    
    if (message_received) {
      size_t recv_len = last_received_len.load();
      if (recv_len == len) {
        std::cout << "✅ 接收成功\n";
      } else {
        std::cout << "❌ 长度不匹配 (收到:" << recv_len << ")\n";
        error_count++;
      }
    } else {
      std::cout << "❌ 超时未收到\n";
      error_count++;
    }
    
    std::this_thread::sleep_for(50ms);
  }
  
  // 完整遍历测试
  if (max_len <= 128) {
    std::cout << "\n遍历所有长度 (1-" << max_len << ")...\n";
    int passed = 0;
    int failed = 0;
    
    for (size_t len = 1; len <= max_len; ++len) {
      uint8_t pattern = (len & 0xFF);
      
      uros::ByteStream msg;
      fill_data(msg.data, len, pattern);
      msg.setLen(len);
      
      message_received = false;
      pub->publish(msg);
      sent_count++;
      
      auto start = std::chrono::steady_clock::now();
      while (!message_received && 
             (std::chrono::steady_clock::now() - start) < 500ms) {
        std::this_thread::sleep_for(5ms);
      }
      
      if (message_received && last_received_len.load() == len) {
        passed++;
      } else {
        failed++;
        std::cout << "  ❌ 长度 " << len << " 失败\n";
      }
      
      // 进度显示
      if (len % 10 == 0) {
        std::cout << "." << std::flush;
      }
    }
    
    std::cout << "\n\n遍历测试结果: " << passed << " 通过, " 
              << failed << " 失败 (共 " << max_len << " 次)\n";
  }
  
  // 停止线程
  running = false;
  receiver_thread.join();
  sender_thread.join();
  
  // 统计
  std::cout << "\n" << std::string(50, '=') << "\n";
  std::cout << "📊 测试统计\n";
  std::cout << std::string(50, '=') << "\n";
  std::cout << "发送: " << sent_count.load() << "\n";
  std::cout << "接收: " << received_count.load() << "\n";
  std::cout << "错误: " << error_count.load() << "\n";
  std::cout << std::string(50, '=') << "\n";
  
  if (error_count == 0 && received_count == sent_count) {
    std::cout << "\n🎉 所有测试通过!\n";
    std::cout << "✅ ByteStream 支持 1-" << max_len << " 字节的变长数据\n\n";
    return 0;
  } else {
    std::cout << "\n⚠️  测试失败\n\n";
    return 1;
  }
}
