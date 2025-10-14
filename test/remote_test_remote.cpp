#include <iostream>
#include <thread>

#include "urosv2/transport_remote_socket.hpp"

#include "urosv2/uros.h"

using namespace std::chrono_literals;

struct MessageHello : uros::MsgBase {
  int32_t seq;
};

struct MessageResponse : uros::MsgBase {
  int32_t seq;
};

void uros_init() {
  uros::RegisterTopic<MessageHello>("/hello", 0);
  uros::RegisterTopic<MessageResponse>("/hello_response", 1);

  // config each transport
  auto transport_local = uros::GetTransportLocal();
  transport_local->declareTopic("/hello");
  transport_local->declareTopic("/hello_response");

  auto transport_udp = uros::RegisterTransport<uros::TransportRemoteSocket>();
  transport_udp->initSocket("127.0.0.1", 10002, "127.0.0.1", 10001);
  transport_udp->declareTopic("/hello");
  // transport_udp->declareTopic("/hello_response");

  uros::Init();
}

int main() {
  uros_init();

  uros::Node node_talker;
  // auto talker_pub = node_talker.createPublisher<MessageHello>("/hello");
  // assert(talker_pub);

  auto talker_sub = node_talker.createSubscription<MessageResponse>(
      "/hello_response", [](const MessageResponse &msg) {
        std::cout << "receive 'Response': " << msg.seq << std::endl;
      });

  uros::Node node_listener;
  // auto listener_pub =
  //     node_listener.createPublisher<MessageResponse>("/hello_response");
  // assert(listener_pub);

  auto listener_sub = node_listener.createSubscription<MessageHello>(
      "/hello", [&](const MessageHello &msg) {
        std::cout << "receive 'Hello': " << msg.seq << std::endl;
        // listener_pub->publish(MessageResponse{.seq = msg.seq});
      });
  assert(listener_sub);

  auto talker_thread = std::thread([&]() {
    std::cout << "talker_thread started" << std::endl;
    while (true) {
      node_talker.spinOnce();
    }
  });

  auto listener_thread = std::thread([&]() {
    std::cout << "listener_thread started" << std::endl;
    node_listener.spin();
  });

  msleep(1000);

  MessageHello hello = {.seq = 0};
  while (true) {
    // std::cout << "pubish 'Hello': " << hello.seq << std::endl;
    // talker_pub->publish(hello);
    // ++hello.seq;

    std::this_thread::sleep_for(1ms);
  }

  return 0;
}
