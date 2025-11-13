#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <vector>

#include "uros/transport_socket.hpp"
#include "uros/uros.h"

using namespace std::chrono_literals;

// 统计信息
std::atomic<int> sent_count{0};
std::atomic<int> received_ack{0};

// 生成测试数据
void fill_data(uint8_t* data, size_t len, uint8_t pattern) {
  for (size_t i = 0; i < len; ++i) {
    data[i] = (pattern + i) & 0xFF;
  }
}

void uros_init() {
  uros::InitGuard g(0);

  uros::RegisterTopic<uros::ByteStream>("/bytestream_data");
  uros::RegisterTopic<uros::ByteStream>("/bytestream_ack");

  // 配置 UDP Transport
  auto transport_udp = uros::RegisterTransport<uros::TransportSocket>();
  transport_udp->initSocket("127.0.0.1", 10001, "127.0.0.1", 10002);
  transport_udp->declareTopic("/bytestream_data");
  transport_udp->declareTopic("/bytestream_ack");
}

int main() {
  std::cout << "\n";
  std::cout << "╔═══════════════════════════════════════════════════╗\n";
  std::cout << "║  ByteStream 跨 Transport 测试 - LOCAL (发送方)  ║\n";
  std::cout << "╚═══════════════════════════════════════════════════╝\n\n";
  
  uros_init();
  
  const size_t max_len = UROS_MSG_MAX_SIZE - sizeof(uros::MsgBase);
  std::cout << "最大数据长度: " << max_len << " 字节\n";
  std::cout << "Transport: UDP Socket (127.0.0.1:10001 -> 127.0.0.1:10002)\n\n";
  
  // 创建节点
  uros::Node sender_node;
  
  // 创建发布者（发送数据）
  auto pub = sender_node.createPublisher<uros::ByteStream>("/bytestream_data");
  if (!pub) {
    std::cerr << "❌ 创建 Publisher 失败\n";
    return 1;
  }
  
  // 创建订阅者（接收ACK）
  auto sub = sender_node.createSubscription<uros::ByteStream>(
      "/bytestream_ack", [](const uros::ByteStream &msg) {
        received_ack++;
        size_t len = msg.getLen();
        uint8_t pattern = (len > 0) ? msg.data[0] : 0;
        std::cout << "  ✅ 收到 ACK - 长度: " << len 
                  << ", pattern: 0x" << std::hex << std::setw(2) 
                  << std::setfill('0') << (int)pattern << std::dec << "\n";
      });
  
  if (!sub) {
    std::cerr << "❌ 创建 Subscription 失败\n";
    return 1;
  }
  
  // 启动线程
  std::atomic<bool> running{true};
  std::thread spin_thread([&]() {
    while (running) {
      sender_node.spinOnce();
      std::this_thread::sleep_for(10ms);
    }
  });
  
  // 等待连接建立
  std::cout << "等待 Transport 连接建立...\n";
  std::this_thread::sleep_for(2s);
  std::cout << "开始发送测试数据\n\n";
  
  // 测试不同长度
  std::vector<size_t> test_lengths = {
    1, 2, 4, 8, 16, 32, 64, 96, 120
  };
  
  for (size_t len : test_lengths) {
    if (len > max_len) {
      std::cout << "跳过长度 " << len << " (超过最大值)\n";
      continue;
    }
    
    uint8_t pattern = (len & 0xFF);
    
    // 准备消息
    uros::ByteStream msg;
    fill_data(msg.data, len, pattern);
    msg.setLen(len);
    
    // 发送
    std::cout << "📤 发送长度 " << std::setw(3) << len 
              << " (pattern: 0x" << std::hex << std::setw(2) 
              << std::setfill('0') << (int)pattern << std::dec << ") ... ";
    std::cout.flush();
    
    int ack_before = received_ack.load();
    pub->publish(msg);
    sent_count++;
    
    // 等待 ACK (最多2秒)
    auto start = std::chrono::steady_clock::now();
    bool got_ack = false;
    while ((std::chrono::steady_clock::now() - start) < 2s) {
      if (received_ack.load() > ack_before) {
        got_ack = true;
        break;
      }
      std::this_thread::sleep_for(10ms);
    }
    
    if (!got_ack) {
      std::cout << "⏱️  未收到 ACK (超时)\n";
    }
    
    std::this_thread::sleep_for(100ms);
  }
  
  // 遍历测试
  if (max_len <= 128) {
    std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    std::cout << "遍历测试 (1-" << max_len << " 字节)\n";
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";
    
    int ack_before = received_ack.load();
    
    for (size_t len = 1; len <= max_len; ++len) {
      uint8_t pattern = (len & 0xFF);
      
      uros::ByteStream msg;
      fill_data(msg.data, len, pattern);
      msg.setLen(len);
      
      pub->publish(msg);
      sent_count++;
      
      if (len % 10 == 0) {
        std::cout << "." << std::flush;
        std::this_thread::sleep_for(50ms);
      } else {
        std::this_thread::sleep_for(20ms);
      }
    }
    
    std::cout << "\n\n等待所有 ACK...\n";
    
    // 等待所有ACK（最多10秒）
    auto start = std::chrono::steady_clock::now();
    while ((std::chrono::steady_clock::now() - start) < 10s) {
      if (received_ack.load() >= ack_before + max_len) {
        break;
      }
      std::this_thread::sleep_for(100ms);
    }
    
    int acks_received = received_ack.load() - ack_before;
    std::cout << "收到 " << acks_received << "/" << max_len << " 个 ACK\n";
  }
  
  // 等待一段时间以便接收所有ACK
  std::this_thread::sleep_for(2s);
  
  // 停止线程
  running = false;
  spin_thread.join();
  
  // 统计
  std::cout << "\n" << std::string(50, '=') << "\n";
  std::cout << "📊 测试统计\n";
  std::cout << std::string(50, '=') << "\n";
  std::cout << "发送消息数: " << sent_count.load() << "\n";
  std::cout << "收到 ACK 数: " << received_ack.load() << "\n";
  std::cout << "ACK 比率:   " << std::fixed << std::setprecision(1)
            << (100.0 * received_ack.load() / sent_count.load()) << "%\n";
  std::cout << std::string(50, '=') << "\n\n";
  
  if (received_ack.load() >= sent_count.load() * 0.9) {
    std::cout << "✅ 跨 Transport 测试成功!\n\n";
    return 0;
  } else {
    std::cout << "⚠️  部分消息未收到 ACK\n\n";
    return 1;
  }
}
