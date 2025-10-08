# Remote Transport Test - 三进程数据流测试

## 测试架构

```
Producer (10001)  →  Relay (10002/10004)  →  Consumer (10003)
  生成数据              自动转发数据              接收数据
```

- **Producer**: 模拟传感器，每2秒生成一次温湿度数据
- **Relay**: **自动转发** - 只需订阅同一个 topic，数据自动在两个 transport 间流动
- **Consumer**: 接收并显示传感器数据

## 核心设计理念

**uros 的自动转发机制**：
- Relay 有两个 TransportRemoteSocket，都声明了同一个 topic `/sensor_data`
- 当 Transport 1 接收到数据时，会自动发布到所有订阅了该 topic 的其他 transport
- Transport 2 自动接收并转发，无需手动编写转发逻辑！

## 数据流

1. Producer 发送 `SensorData` → UDP 10002
2. Relay 的 Transport 1 接收 → **自动转发**到 Transport 2 → UDP 10003
3. Consumer 接收并打印

**关键点**：Relay 不需要手动调用 publish，uros 框架会自动处理跨 transport 的消息路由！

## 编译

```bash
cd /home/mr/workspace/uros

# 编译 Producer
g++ -std=c++17 -I. -Itest test/remote_test_producer.cpp -o build/remote_producer -pthread

# 编译 Relay
g++ -std=c++17 -I. -Itest test/remote_test_relay.cpp -o build/remote_relay -pthread

# 编译 Consumer
g++ -std=c++17 -I. -Itest test/remote_test_consumer.cpp -o build/remote_consumer -pthread

# 或者一次性编译全部
g++ -std=c++17 -I. -Itest test/remote_test_producer.cpp -o build/remote_producer -pthread && \
g++ -std=c++17 -I. -Itest test/remote_test_relay.cpp -o build/remote_relay -pthread && \
g++ -std=c++17 -I. -Itest test/remote_test_consumer.cpp -o build/remote_consumer -pthread
```

## 运行

**需要按顺序在三个独立的终端中运行：**

### 终端1: 启动 Consumer (先启动接收端)
```bash
./build/remote_consumer
```
输出示例：
```
=== Data Consumer (Port 10003) ===
Consumer node ready, waiting for data from Relay...
```

### 终端2: 启动 Relay (中转节点)
```bash
./build/remote_relay
```
输出示例：
```
=== Data Relay (Port 10002 <- Producer, Port 10004 -> Consumer) ===
Relay node ready, waiting for data from Producer...
```

### 终端3: 启动 Producer (数据源)
```bash
./build/remote_producer
```
输出示例：
```
=== Data Producer (Port 10001) ===
Producer thread started
Starting to produce sensor data...
📤 Producing: seq=1, temp=25.5°C, humidity=60.3%
📤 Producing: seq=2, temp=26°C, humidity=60.6%
```

## 预期输出

### Producer 输出:
```
📤 Producing: seq=1, temp=25.5°C, humidity=60.3%
📤 Producing: seq=2, temp=26°C, humidity=60.6%
📤 Producing: seq=3, temp=26.5°C, humidity=60.9%
```

### Relay 输出:
```
🔄 Relaying: seq=1, temp=25.5°C, humidity=60.3%
🔄 Relaying: seq=2, temp=26°C, humidity=60.6%
🔄 Relaying: seq=3, temp=26.5°C, humidity=60.9%
(数据自动转发，无需手动 publish!)
```

### Consumer 输出:
```
📥 Received sensor data:
   Sensor ID: 1001
   Sequence:  1
   Temp:      25.5°C
   Humidity:  60.3%
   ----------------------------------------
📥 Received sensor data:
   Sensor ID: 1001
   Sequence:  2
   Temp:      26°C
   Humidity:  60.6%
   ----------------------------------------
```

## Transport 配置

### Producer
- TransportLocal: 本地通信
- TransportRemoteSocket: 127.0.0.1:10001 → 127.0.0.1:10002

### Relay
- TransportLocal: 本地通信
- TransportRemoteSocket (in): 127.0.0.1:10002 ← 127.0.0.1:10001
- TransportRemoteSocket (out): 127.0.0.1:10004 → 127.0.0.1:10003

### Consumer
- TransportLocal: 本地通信
- TransportRemoteSocket: 127.0.0.1:10003 ← 127.0.0.1:10004

## Topic 定义

### /sensor_data (topic_id=0)
```cpp
struct SensorData : uros::MsgBase {
  int32_t sensor_id;      // 传感器ID
  int32_t sequence;       // 序列号
  float temperature;      // 温度(°C)
  float humidity;         // 湿度(%)
};
```

### /processed_data (topic_id=1)
```cpp
struct ProcessedData : uros::MsgBase {
  int32_t sensor_id;
  int32_t sequence;
  float temperature_celsius;
  float temperature_fahrenheit;  // Relay添加
  float humidity;
  int32_t relay_timestamp;       // Relay添加
};
```

## 故障排查

如果消息没有收到：

1. **检查端口占用**:
   ```bash
   netstat -an | grep -E "10001|10002|10003|10004"
   ```

2. **检查启动顺序**: 必须先启动 Consumer，再启动 Relay，最后启动 Producer

3. **查看 verbose 输出**: 在 `define.h` 中确保 `UROS_VERBOSE` 已定义

4. **防火墙检查**: 确保 localhost 的 UDP 通信没有被阻止
