#include <iostream>
#include <thread>
#include <chrono>

#include "uros/uros.h"
#include "uros/transport_local.h"
#include "uros/transport_remote_socket.h"

using namespace std::chrono_literals;

// 使用 TransportLocal + TransportRemoteSocket
using Uros = uros::System<uros::TransportLocal, uros::TransportRemoteSocket>;

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
  
  // Transport 1: Producer 监听 10001，发送到 Relay 的 10002
  auto &transport_remote = Uros::Transport<1>();
  transport_remote.initSocket("127.0.0.1", 10001, "127.0.0.1", 10002);
  transport_remote.declareTopic("/sensor_data");
  
  Uros::Init();
}

int main() {
  std::cout << "=== Data Producer (Port 10001) ===" << std::endl;
  
  uros_init();

  Uros::Node producer_node;
  auto sensor_pub = producer_node.createPublisher<SensorData>("/sensor_data");
  assert(sensor_pub);

  // 生产者线程：处理订阅的消息（这里没有订阅，但保持框架完整）
  auto producer_thread = std::thread([&]() {
    std::cout << "Producer thread started" << std::endl;
    while (true) {
      producer_node.spinOnce();
      std::this_thread::sleep_for(10ms);
    }
  });

  // 主线程：定期发送传感器数据
  SensorData data;
  data.sensor_id = 1001;
  data.sequence = 0;
  data.temperature = 25.0f;
  data.humidity = 60.0f;
  
  std::cout << "Starting to produce sensor data..." << std::endl;
  
  while (true) {
    // 模拟传感器数据变化
    data.sequence++;
    data.temperature = 25.0f + (data.sequence % 10) * 0.5f;
    data.humidity = 60.0f + (data.sequence % 20) * 0.3f;
    
    std::cout << "📤 Producing: seq=" << data.sequence 
              << ", temp=" << data.temperature 
              << "°C, humidity=" << data.humidity << "%" << std::endl;
    
    sensor_pub->publish(data);
    
    std::this_thread::sleep_for(2000ms);
  }

  producer_thread.join();
  return 0;
}
