#include <iostream>
#include <thread>

#include "uros/uros.h"

#include "uros/transport_socket.hpp"

using namespace std::chrono_literals;

struct ReqAdd2 : uros::ReqBase {
  int a;
  int b;
};

struct RspAdd2 : uros::RspBase {
  int c;
};

void uros_init() {
  uros::SetSystemId(0);

  uros::RegisterService<ReqAdd2, RspAdd2>("/add2", 0);

  uros::Init();
}

int main() {
  uros_init();

  uros::Node node_cli;
  auto cli_add2 = node_cli.createClient<ReqAdd2, RspAdd2>("/add2");
  assert(cli_add2);

  uros::Node node_srv;
  auto srv_add2 = node_srv.createServer<ReqAdd2, RspAdd2>(
      "/add2", [](const ReqAdd2 &req, RspAdd2 &rsp) { rsp.c = req.a + req.b; });
  assert(srv_add2);

  auto cli_thread = std::thread([&]() {
    std::cout << "client thread started" << std::endl;
    node_cli.spin();
  });

  auto srv_thread = std::thread([&]() {
    std::cout << "server thread started" << std::endl;
    node_srv.spin();
  });

  msleep(1000);

  ReqAdd2 req;
  req.a = 0;
  req.b = 0;
  while (true) {
    std::cout << "call remote add2: " << req.a << "," << req.b << std::endl;
    RspAdd2 rsp;
    if (cli_add2->call(req, rsp, -1)) {
      std::cout << "call remote add2 succeed: " << rsp.c << std::endl;
    } else {
      std::cout << "call remote add2 failed" << std::endl;
    }

    std::this_thread::sleep_for(1000ms);
    ++req.a;
  }

  return 0;
}
