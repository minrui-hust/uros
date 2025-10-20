#pragma once

#include "client.hpp"
#include "node.hpp"
#include "publisher.hpp"
#include "server.hpp"
#include "service.hpp"
#include "subscription.hpp"
#include "system.h"
#include "topic.hpp"
#include "transport.hpp"

namespace uros {

template <typename Msg>
static auto RegisterTopic(const char *topic_name, const uint8_t topic_id,
                          const uint8_t topic_prio = 0) {
  static_assert(sizeof(Msg) <= UROS_MSG_MAX_SIZE);
  return TopicManager::AddTopic<TopicT<Msg>>(topic_name, topic_id, topic_prio);
}

template <typename Req, typename Rsp>
static auto RegisterService(const char *service_name,
                            const uint8_t service_id) {
  static_assert(sizeof(Req) <= UROS_MSG_MAX_SIZE);
  static_assert(sizeof(Rsp) <= UROS_MSG_MAX_SIZE);
  return ServiceManager::AddService<ServiceT<Req, Rsp>>(service_name,
                                                        service_id);
}

template <typename Transport> static auto RegisterTransport() {
  return TransportManager::AddTransport<Transport>();
}

static void SetSystemId(int sys_id) { System::Id() = sys_id; }

static void Init() {
  // init the transports
  TransportManager::Init();

  // TODO: maybe other work
}

} // namespace uros
