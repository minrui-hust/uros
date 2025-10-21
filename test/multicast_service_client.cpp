#include <chrono>
#include <iostream>
#include <thread>

#include "uros/transport_udp_multicast.hpp"
#include "uros/uros.h"

using namespace std::chrono_literals;

struct ReqAdd2 : uros::ReqBase {
  int a;
  int b;
};

struct RspAdd2 : uros::RspBase {
  int c;
};

void uros_init(int client_id) {
  uros::InitGuard g(client_id);

  uros::RegisterService<ReqAdd2, RspAdd2>("/add2", 0);

  // 配置UDP组播传输
  auto transport_multicast =
      uros::RegisterTransport<uros::TransportUdpMulticast>();

  // 发送端口自动获取
  transport_multicast->initSocket("239.0.0.2");
  transport_multicast->declareService("/add2");
}

int main(int argc, char *argv[]) {
  // 从命令行参数获取客户端ID，默认为1
  int client_id = 1;
  if (argc > 1) {
    client_id = std::atoi(argv[1]);
  }

  std::cout << "=== UDP Multicast Service Client [ID: " << client_id
            << "] ===" << std::endl;
  std::cout << "Multicast: 239.0.0.1:10000" << std::endl;
  std::cout << "Send Port: Auto-assigned" << std::endl;

  uros_init(client_id);

  // 创建客户端节点
  uros::Node node_cli;
  auto cli_add2 = node_cli.createClient<ReqAdd2, RspAdd2>("/add2");
  assert(cli_add2);

  // 启动节点处理线程
  auto cli_thread = std::thread([&]() {
    std::cout << "[CLIENT-" << client_id << "] Node thread started"
              << std::endl;
    node_cli.spin();
  });

  // 等待2秒后开始服务调用
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));

  std::cout << "[CLIENT-" << client_id << "] Starting service calls..."
            << std::endl;

  ReqAdd2 req;
  req.a = client_id;
  req.b = client_id * 2;

  int call_count = 0;
  int success_count = 0;

  while (true) {
    call_count++;

    std::cout << "\n[CLIENT-" << client_id << "] === Call #" << call_count
              << " ===" << std::endl;
    std::cout << "[CLIENT-" << client_id << "] Request: add2(" << req.a << ", "
              << req.b << ")" << std::endl;

    auto start_time = std::chrono::steady_clock::now();

    RspAdd2 rsp;
    bool success = cli_add2->call(req, rsp, 5000); // 5秒超时

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (success) {
      success_count++;
      std::cout << "[CLIENT-" << client_id << "] ✅ Response: " << req.a << " + "
                << req.b << " = " << rsp.c << " (took " << duration.count()
                << "ms)" << std::endl;
    } else {
      std::cout << "[CLIENT-" << client_id << "] ❌ FAILED (timeout after "
                << duration.count() << "ms)" << std::endl;
    }

    std::cout << "[CLIENT-" << client_id
              << "] Success rate: " << (100.0 * success_count / call_count)
              << "% (" << success_count << "/" << call_count << ")"
              << std::endl;

    // 更新请求参数
    req.a += 1;
    req.b += 2;

    // 等待2秒
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }

  return 0;
}
