#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <iomanip>

#include "uros/transport_socket.hpp"
#include "uros/uros.h"

using namespace std::chrono_literals;

// 统计信息
std::atomic<int> received_count{0};
std::atomic<int> sent_ack{0};
std::atomic<int> verify_failed{0};

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
  uros::InitGuard g(0);

  uros::RegisterTopic<uros::ByteStream>("/bytestream_data");
  uros::RegisterTopic<uros::ByteStream>("/bytestream_ack");

  // 配置 UDP Transport (端口与local相反)
  auto transport_udp = uros::RegisterTransport<uros::TransportSocket>();
  transport_udp->initSocket("127.0.0.1", 10002, "127.0.0.1", 10001);
  transport_udp->declareTopic("/bytestream_data");
  transport_udp->declareTopic("/bytestream_ack");
}

int main() {
  std::cout << "\n";
  std::cout << "╔═══════════════════════════════════════════════════╗\n";
  std::cout << "║  ByteStream 跨 Transport 测试 - REMOTE (接收方) ║\n";
  std::cout << "╚═══════════════════════════════════════════════════╝\n\n";
  
  uros_init();
  
  const size_t max_len = UROS_MSG_MAX_SIZE - sizeof(uros::MsgBase);
  std::cout << "最大数据长度: " << max_len << " 字节\n";
  std::cout << "Transport: UDP Socket (127.0.0.1:10002 -> 127.0.0.1:10001)\n\n";
  std::cout << "等待接收数据...\n\n";
  
  // 创建节点
  uros::Node receiver_node;
  
  // 创建发布者（发送ACK）
  auto pub_ack = receiver_node.createPublisher<uros::ByteStream>("/bytestream_ack");
  if (!pub_ack) {
    std::cerr << "❌ 创建 ACK Publisher 失败\n";
    return 1;
  }
  
  // 创建订阅者（接收数据）
  auto sub = receiver_node.createSubscription<uros::ByteStream>(
      "/bytestream_data", [&](const uros::ByteStream &msg) {
        received_count++;
        
        size_t len = msg.getLen();
        uint8_t pattern = (len > 0) ? msg.data[0] : 0;
        
        // 验证数据
        bool valid = verify_data(msg.data, len, pattern);
        
        if (valid) {
          std::cout << "✅ 收到数据 #" << std::setw(3) << received_count.load()
                    << " - 长度: " << std::setw(3) << len 
                    << ", pattern: 0x" << std::hex << std::setw(2) 
                    << std::setfill('0') << (int)pattern << std::dec
                    << " - 验证通过\n";
        } else {
          std::cout << "❌ 收到数据 #" << std::setw(3) << received_count.load()
                    << " - 长度: " << std::setw(3) << len
                    << " - 数据验证失败!\n";
          verify_failed++;
        }
        
        // 发送 ACK (回显相同的数据表示收到)
        uros::ByteStream ack_msg;
        ack_msg.data[0] = pattern;
        ack_msg.setLen(len);
        pub_ack->publish(ack_msg);
        sent_ack++;
      });
  
  if (!sub) {
    std::cerr << "❌ 创建 Subscription 失败\n";
    return 1;
  }
  
  // 启动线程
  std::atomic<bool> running{true};
  std::thread spin_thread([&]() {
    while (running) {
      receiver_node.spinOnce();
      std::this_thread::sleep_for(5ms);
    }
  });
  
  // 运行一段时间（等待local发送完成）
  std::cout << "按 Ctrl+C 停止接收...\n\n";
  
  // 运行30秒后自动停止（或手动Ctrl+C）
  auto start_time = std::chrono::steady_clock::now();
  while ((std::chrono::steady_clock::now() - start_time) < 30s && running) {
    std::this_thread::sleep_for(1s);
  }
  
  // 停止线程
  running = false;
  spin_thread.join();
  
  // 统计
  std::cout << "\n" << std::string(50, '=') << "\n";
  std::cout << "📊 接收统计\n";
  std::cout << std::string(50, '=') << "\n";
  std::cout << "接收消息数:     " << received_count.load() << "\n";
  std::cout << "发送 ACK 数:    " << sent_ack.load() << "\n";
  std::cout << "验证失败数:     " << verify_failed.load() << "\n";
  std::cout << "验证成功率:     " << std::fixed << std::setprecision(1);
  
  if (received_count.load() > 0) {
    std::cout << (100.0 * (received_count.load() - verify_failed.load()) / received_count.load()) << "%\n";
  } else {
    std::cout << "0.0%\n";
  }
  
  std::cout << std::string(50, '=') << "\n\n";
  
  if (verify_failed.load() == 0 && received_count.load() > 0) {
    std::cout << "✅ 所有接收的数据验证通过!\n\n";
    return 0;
  } else if (received_count.load() == 0) {
    std::cout << "⚠️  未收到任何数据\n\n";
    return 1;
  } else {
    std::cout << "⚠️  部分数据验证失败\n\n";
    return 1;
  }
}
