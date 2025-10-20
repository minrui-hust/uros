#include <iostream>
#include <thread>

#include "uros/transport_socket.hpp"
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
  uros::SetSystemId(1); // 远程端使用不同的系统ID

  uros::RegisterService<ReqAdd2, RspAdd2>("/add2", 0);

  // 配置远程传输 - 作为服务端
  auto transport_udp = uros::RegisterTransport<uros::TransportSocket>();
  transport_udp->initSocket("127.0.0.1", 10002, "127.0.0.1", 10001);
  transport_udp->declareService("/add2");

  uros::Init();
}

int main() {
  uros_init();

  std::cout << "Remote Service Server started on 127.0.0.1:10002" << std::endl;

  // 创建服务端节点和服务
  uros::Node node_srv;
  auto srv_add2 = node_srv.createServer<ReqAdd2, RspAdd2>(
      "/add2", [](const ReqAdd2 &req, RspAdd2 &rsp) {
        std::cout << "[SERVER] Service called with: " << req.a << " + " << req.b
                  << std::endl;
        rsp.c = req.a + req.b;
        std::this_thread::sleep_for(10ms);
        std::cout << "[SERVER] Service response: " << rsp.c << std::endl;
      });
  assert(srv_add2);

  // 创建本地客户端节点和客户端
  uros::Node node_cli;
  auto cli_add2 = node_cli.createClient<ReqAdd2, RspAdd2>("/add2");
  assert(cli_add2);

  // 服务端线程
  auto srv_thread = std::thread([&]() {
    std::cout << "server thread started" << std::endl;
    node_srv.spin();
  });

  // 客户端线程
  auto cli_thread = std::thread([&]() {
    std::cout << "client thread started" << std::endl;
    node_cli.spin();
  });

  std::cout << "Remote service server is running..." << std::endl;
  std::cout << "Waiting for service calls from remote client..." << std::endl;
  std::cout << "Also making local service calls..." << std::endl;

  // 等待服务启动
  msleep(1000);

  // 本地客户端定期调用本地服务
  ReqAdd2 local_req;
  local_req.a = 100;
  local_req.b = 200;

  while (true) {
    std::cout << "\n=== [LOCAL CLIENT] Calling local add2 service ==="
              << std::endl;
    std::cout << "[LOCAL CLIENT] Request: add2(" << local_req.a << ", "
              << local_req.b << ")" << std::endl;

    RspAdd2 local_rsp;
    if (cli_add2->call(local_req, local_rsp, 1000)) { // 1秒超时
      std::cout << "[LOCAL CLIENT] Response: " << local_req.a << " + "
                << local_req.b << " = " << local_rsp.c << std::endl;
      std::cout << "[LOCAL CLIENT] Local service call SUCCEEDED" << std::endl;
    } else {
      std::cout << "[LOCAL CLIENT] Local service call FAILED" << std::endl;
    }

    // 递增参数
    local_req.a += 5;
    local_req.b += 5;

    // 等待1秒后再次调用
    std::this_thread::sleep_for(1000ms);
  }

  return 0;
}
