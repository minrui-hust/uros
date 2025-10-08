#include <iostream>
#include <thread>
#include <chrono>

#include "uros/uros.h"
#include "uros/transport_remote_socket.h"

using namespace std::chrono_literals;

// 使用 TransportLocal + TransportRemoteSocket
using Uros = uros::System<uros::TransportLocal, uros::TransportRemoteSocket>;

void uros_init() {
  // Transport 0: TransportLocal
  auto &transport_local = Uros::Transport<0>();
  transport_local.declareTopic("/sensor_data", 0);
  
  // Transport 1: 从 Relay (10003) 接收数据，监听 10004
  auto &transport = Uros::Transport<1>();
  transport.initSocket("127.0.0.1", 10004, "127.0.0.1", 10003);
  transport.declareTopic("/sensor_data", 0);
  
  Uros::Init();
}

// 传感器数据消息（与 Producer 定义一致）
struct SensorData : uros::MsgBase {
  int32_t sensor_id;
  int32_t sequence;
  float temperature;
  float humidity;
};

int main() {
  std::cout << "=== Data Consumer (Port 10004) ===" << std::endl;
  
  uros_init();

  Uros::Node consumer_node;
  
  // 订阅传感器数据
  auto data_sub = consumer_node.createSubscription<SensorData>(
      "/sensor_data", [](const SensorData &msg) {
        std::cout << "📥 Received sensor data:" << std::endl;
        std::cout << "   Sensor ID: " << msg.sensor_id << std::endl;
        std::cout << "   Sequence:  " << msg.sequence << std::endl;
        std::cout << "   Temp:      " << msg.temperature << "°C" << std::endl;
        std::cout << "   Humidity:  " << msg.humidity << "%" << std::endl;
        std::cout << "   ----------------------------------------" << std::endl;
      });
  assert(data_sub);

  std::cout << "Consumer node ready, waiting for data from Relay..." << std::endl;
  
  // Consumer 线程：处理接收到的消息
  consumer_node.spin();

  return 0;
}
