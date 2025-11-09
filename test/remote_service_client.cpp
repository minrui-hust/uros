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
  uros::InitGuard g(0);

  uros::RegisterService<ReqAdd2, RspAdd2>("/add2");

  // 配置远程传输 - 作为客户端
  auto transport_udp = uros::RegisterTransport<uros::TransportSocket>();
  transport_udp->initSocket("127.0.0.1", 10001, "127.0.0.1", 10002);
  transport_udp->declareService("/add2");
}

int main() {
  uros_init();

  std::cout << "Remote Service Client started on 127.0.0.1:10001" << std::endl;
  std::cout << "Connecting to remote service server at 127.0.0.1:10002"
            << std::endl;

  // 只创建客户端，不创建服务端
  uros::Node node_cli;
  auto cli_add2 = node_cli.createClient<ReqAdd2, RspAdd2>("/add2");
  assert(cli_add2);

  auto cli_thread = std::thread([&]() {
    std::cout << "client thread started" << std::endl;
    node_cli.spin();
  });

  // 等待连接建立
  msleep(1000);

  ReqAdd2 req;
  req.a = 1;
  req.b = 2;

  std::cout << "Starting remote service calls..." << std::endl;

  while (true) {
    std::cout << "\n=== Calling remote service ===" << std::endl;
    std::cout << "Request: add2(" << req.a << ", " << req.b << ")" << std::endl;

    RspAdd2 rsp;
    if (cli_add2->call(req, rsp, 1000)) { // 5秒超时
      std::cout << "Response: " << req.a << " + " << req.b << " = " << rsp.c
                << std::endl;
      std::cout << "Remote service call SUCCEEDED" << std::endl;
    } else {
      std::cout << "Remote service call FAILED (timeout or error)" << std::endl;
    }

    // 递增参数以便测试
    req.a++;
    req.b++;

    // 等待一段时间后继续下一次调用
    std::this_thread::sleep_for(1000ms);
  }

  return 0;
}
