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

void uros_init() {
  uros::InitGuard g(0);

  uros::RegisterService<ReqAdd2, RspAdd2>("/add2", 0);

  // 配置UDP组播传输
  auto transport_multicast =
      uros::RegisterTransport<uros::TransportUdpMulticast>();
  transport_multicast->initSocket("239.0.0.2");
  transport_multicast->declareService("/add2");
}

int main() {
  uros_init();

  std::cout << "=== UDP Multicast Service Server ===" << std::endl;
  std::cout << "Multicast: 239.0.0.1:10000" << std::endl;
  std::cout << "Send Port: Auto-assigned" << std::endl;

  // 创建服务端节点和服务
  uros::Node node_srv;
  auto srv_add2 = node_srv.createServer<ReqAdd2, RspAdd2>(
      "/add2", [](const ReqAdd2 &req, RspAdd2 &rsp) {
        auto tid = std::this_thread::get_id();
        std::cout << "[SERVER-" << tid << "] Service called: " << req.a << " + "
                  << req.b << std::endl;

        rsp.c = req.a + req.b;

        // 模拟处理时间
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        std::cout << "[SERVER-" << tid << "] Response: " << rsp.c << std::endl;
      });
  assert(srv_add2);

  // 创建本地客户端节点和客户端
  uros::Node node_cli;
  auto cli_add2 = node_cli.createClient<ReqAdd2, RspAdd2>("/add2");
  assert(cli_add2);

  // 服务端线程
  auto srv_thread = std::thread([&]() {
    std::cout << "[THREAD] Server thread started" << std::endl;
    node_srv.spin();
  });

  // 客户端线程
  auto cli_thread = std::thread([&]() {
    std::cout << "[THREAD] Client thread started" << std::endl;
    node_cli.spin();
  });

  std::cout << "UDP Multicast service server is running..." << std::endl;
  std::cout << "- Accepting remote service calls via multicast" << std::endl;
  std::cout << "- Making local service calls periodically" << std::endl;

  // 等待服务启动
  msleep(2000);

  // 本地客户端定期调用本地服务
  ReqAdd2 local_req;
  local_req.a = 1000;
  local_req.b = 2000;

  while (true) {
    std::cout << "\n=== [LOCAL CLIENT] Self-testing ===" << std::endl;
    std::cout << "[LOCAL CLIENT] Request: add2(" << local_req.a << ", "
              << local_req.b << ")" << std::endl;

    RspAdd2 local_rsp;
    if (cli_add2->call(local_req, local_rsp, 1000)) { // 2秒超时
      std::cout << "[LOCAL CLIENT] Response: " << local_req.a << " + "
                << local_req.b << " = " << local_rsp.c << std::endl;
      std::cout << "[LOCAL CLIENT] ✅ SUCCESS" << std::endl;
    } else {
      std::cout << "[LOCAL CLIENT] ❌ FAILED" << std::endl;
    }

    // 递增参数
    local_req.a += 10;
    local_req.b += 5;

    // 等待3秒后再次调用
    std::this_thread::sleep_for(1000ms);
  }

  return 0;
}
