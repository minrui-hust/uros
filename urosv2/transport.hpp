#pragma once

#include "service.h"
#include "topic.h"
#include "transport.h"

namespace uros {

inline bool TransportBase::declareTopic(const char *topic_name) {
  auto topic = TopicManager::FindTopic<TopicBase>(topic_name);
  if (!topic) {
    return false;
  }

  auto topic_id = topic->id();
  if ((size_t)topic_id >= topic_metas_.size()) {
    return false;
  }

  auto &meta = topic_metas_[topic_id];
  if (meta) {
    UROS_PRINT("topic '%s' already declared on transport %d \n", topic_name,
               id_);
    return false;
  }

  meta.reset(new TopicMeta);
  meta->topic = topic;
  topic->addTransport(this);

  topic_bit_mask_ |= 1 << topic_id;

  UROS_PRINT("add topic '%s' to transport %d succeed\n", topic_name, id_);

  return true;
}

inline bool TransportBase::declareService(const char *service_name) {
  // find service by name
  auto service = ServiceManager::FindService<ServiceBase>(service_name);
  if (!service) {
    return false;
  }

  // check if service id exceed limit
  auto service_id = service->id();
  if ((size_t)service_id >= services_.size()) {
    return false;
  }

  if (!services_[service_id]) {
    services_[service_id] = service; // register service
  } else {
    UROS_PRINT("service '%s' already declared on transport %d \n", service_name,
               id_);
  }

  UROS_PRINT("add service '%s' to transport %d succeed\n", service_name, id_);

  return true;
}

inline void TransportBase::init() {
  send_worker_ = std::make_unique<Thread>(
      "transport_send_worker", UROS_TRANSPORT_WORKER_STACK_DEPTH,
      UROS_TRANSPORT_WORKER_PRIORITY, [&]() { sendWork(); });
  CHECK(send_worker_);

  recv_worker_ = std::make_unique<Thread>(
      "transport_recv_worker", UROS_TRANSPORT_WORKER_STACK_DEPTH,
      UROS_TRANSPORT_WORKER_PRIORITY, [&]() { recvWork(); });
  CHECK(recv_worker_);
}

inline void TransportBase::notify(TopicBase *topic) {
  ThreadNotify(send_worker_.get(), 1 << topic->id());
}

inline void TransportBase::sendWork() {
  uint32_t flags;
  while (true) {
    ThreadNotifyWait(0, topic_bit_mask_, &flags, -1); // blocking wait
    UROS_PRINT("TransportBase::sendWork: wait done, 0x%x\n", flags);

    for (auto i = 0u; i < topic_metas_.size(); ++i) {
      if (flags & (1 << i)) {
        auto &meta = topic_metas_[i];
        if (meta->topic->read(&send_buf_.msg, meta->seq)) {
          send(&send_buf_.msg, meta->topic->msgSize(), meta->topic->prio(), -1);
        }
      }
    }
  }
}

inline void TransportBase::recvWork() {
  int prio;
  while (true) {
    auto len = recv(recv_buf_.data, sizeof(recv_buf_), &prio, -1);
    if (len < (int)sizeof(MsgMeta)) {
      continue;
    }

    auto msg = &recv_buf_.msg;
    if (msg->__meta__.type == MsgType::MsgTypeNormal) {
      recvNormal(msg);
    } else if (msg->__meta__.type == MsgType::MsgTypeRequest) {
      recvRequest(msg);
    } else if (msg->__meta__.type == MsgType::MsgTypeResponse) {
      recvResponse(msg);
    } else if (msg->__meta__.type == MsgType::MsgTypeServiceBroadcast) {
      recvServiceBroadcast(msg);
    } else if (msg->__meta__.type == MsgType::MsgTypeServiceDiscovery) {
      recvServiceDiscovery(msg);
    } else {
      UROS_PRINT("Unknow msg type: %d\n", msg->__meta__.type);
    }
  }
}

inline void TransportBase::recvNormal(const MsgBase *msg) {
  UROS_PRINT("TransportBase::recvNormal, topic_id: %d\n",
             msg->__meta__.id.msg.topic);
  auto topic_id = msg->__meta__.id.msg.topic;
  if (topic_id >= topic_metas_.size() || !topic_metas_[topic_id]) {
    return;
  }

  auto &meta = topic_metas_[topic_id];

  meta->topic->write(msg, this);
}

inline void TransportBase::recvRequest(const MsgBase *msg) {
  // TODO
}

inline void TransportBase::recvResponse(const MsgBase *msg) {
  // TODO
}

inline void TransportBase::recvServiceBroadcast(const MsgBase *msg) {
  // TODO
}

inline void TransportBase::recvServiceDiscovery(const MsgBase *msg) {
  // TODO
}

template <typename Transport> Transport *TransportManager::addTransport() {
  if (tsps_.full()) {
    return nullptr;
  }

  auto tsp = new Transport();
  assert(tsp);

  tsp->id() = tsps_.size();

  tsps_.emplace_back(tsp);

  return tsp;
}

inline void TransportManager::init() {
  for (auto &tsp : tsps_) {
    tsp->init();
  }
}

} // namespace uros
