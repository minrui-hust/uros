#include <iostream>
#include <thread>

#include "uros/uros.h"

#include "uros/transport_remote_socket.h"

using namespace std::chrono_literals;

using Uros = uros::System<uros::TransportLocal, uros::TransportRemoteSocket>;

void uros_init() {
  // config each transport
  auto &transport_local = Uros::Transport<0>();
  transport_local.declareTopic("/hello", 0);
  transport_local.declareTopic("/hello_response", 1);

  auto &transport_udp = Uros::Transport<1>();
  transport_udp.initSocket("127.0.0.1", 10001, "127.0.0.1", 10000);

  transport_udp.declareTopic("/hello", 0);
  transport_udp.declareTopic("/hello_response", 1);

  Uros::Init();
}

struct MessageHello : uros::MsgBase {
  int32_t seq;
};

struct MessageResponse : uros::MsgBase {
  int32_t id;
  int32_t seq;
};

int main() {
  uros_init();

  Uros::Node node_listener;
  auto listener_pub =
      node_listener.createPublisher<MessageResponse>("/hello_response");
  assert(listener_pub);

  auto listener_sub = node_listener.createSubscription<MessageHello>(
      "/hello", [&](const MessageHello &msg) {
        std::cout << "listenter receive 'Hello': " << msg.seq << std::endl;
        listener_pub->publish(MessageResponse{.id = 1, .seq = msg.seq});
      });
  assert(listener_sub);

  auto listener_thread = std::thread([&]() {
    std::cout << "listener_thread started" << std::endl;
    node_listener.spin();
  });

  while (true) {
    std::this_thread::sleep_for(1000ms);
  }

  return 0;
}
