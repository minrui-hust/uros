#include <iostream>
#include <thread>
#include <chrono>

#include "uros/system.h"
#include "uros/transport_local.h"
#include "uros/transport_remote_socket.h"

using namespace std::chrono_literals;

// 使用 TransportLocal + 两个 TransportRemoteSocket：一个接收，一个转发
using Uros = uros::System<uros::TransportLocal, uros::TransportRemoteSocket, uros::TransportRemoteSocket>;

// 传感器数据消息
struct SensorData : uros::MsgBase {
  int32_t sensor_id;
  int32_t sequence;
  float temperature;
  float humidity;
};

void uros_init() {
  // 注册 topic
  Uros::RegisterTopic<SensorData>("/sensor_data", 0);
  
  // Transport 0: TransportLocal
  auto &transport_local = Uros::Transport<0>();
  transport_local.declareTopic("/sensor_data");
  
  // Transport 1: 从 Producer (10001) 接收数据，监听 10002
  auto &transport_in = Uros::Transport<1>();
  transport_in.initSocket("127.0.0.1", 10002, "127.0.0.1", 10001);
  transport_in.declareTopic("/sensor_data");
  
  // Transport 2: 转发数据到 Consumer (10004)，监听 10003
  auto &transport_out = Uros::Transport<2>();
  transport_out.initSocket("127.0.0.1", 10003, "127.0.0.1", 10004);
  transport_out.declareTopic("/sensor_data");  // 同一个 topic！
  
  Uros::Init();
}

int main() {
  std::cout << "=== Data Relay (Port 10002 <- Producer, Port 10003 -> Consumer) ===" << std::endl;
  
  uros_init();

  Uros::Node relay_node;
  
  // 订阅 /sensor_data，仅用于监控（可选）
  // auto relay_sub = relay_node.createSubscription<SensorData>(
  //     "/sensor_data", [](const SensorData &msg) {
  //       std::cout << "🔄 Relaying: seq=" << msg.sequence 
  //                 << ", temp=" << msg.temperature 
  //                 << "°C, humidity=" << msg.humidity << "%" << std::endl;
  //     });
  // assert(relay_sub);

  std::cout << "Relay node ready, auto-forwarding /sensor_data..." << std::endl;
  std::cout << "  Transport 1 (in):  127.0.0.1:10002 <- Producer" << std::endl;
  std::cout << "  Transport 2 (out): 127.0.0.1:10003 -> Consumer" << std::endl;
  std::cout << "  Data will be automatically forwarded!" << std::endl;
  
  // Relay 线程：处理消息（数据会自动转发）
  relay_node.spin();

  return 0;
}
