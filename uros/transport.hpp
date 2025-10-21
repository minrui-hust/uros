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
  topic->registerTransport(this);

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
  if ((size_t)service_id >= service_metas_.size()) {
    return false;
  }

  auto &meta = service_metas_[service_id];
  if (meta) {
    UROS_PRINT("service '%s' already declared on transport %d \n", service_name,
               id_);
    return false;
  }

  meta.reset(new ServiceMeta);
  meta->service = service;
  service->registerTransport(this);

  UROS_PRINT("add service '%s' to transport %d succeed\n", service_name, id_);

  return true;
}

inline void TransportBase::init() {
  send_worker_ = std::make_unique<Thread>(
      "tsp_send_worker", UROS_TRANSPORT_WORKER_STACK_DEPTH,
      UROS_TRANSPORT_WORKER_PRIORITY, [&]() { sendWork(); });
  CHECK(send_worker_);

  recv_worker_ = std::make_unique<Thread>(
      "tsp_recv_worker", UROS_TRANSPORT_WORKER_STACK_DEPTH,
      UROS_TRANSPORT_WORKER_PRIORITY, [&]() { recvWork(); });
  CHECK(recv_worker_);

  service_worker_ = std::make_unique<Thread>(
      "tsp_service_worker", UROS_TRANSPORT_WORKER_STACK_DEPTH,
      UROS_TRANSPORT_WORKER_PRIORITY, [&]() { serviceWork(); });
  CHECK(service_worker_);
}

template <typename Topic> void TransportBase::notify(Topic *topic) {
  ThreadNotify(send_worker_.get(), 1 << topic->id());
}

template <typename Service>
bool TransportBase::sendReq(Service *service, const typename Service::Req &req,
                            int timeout_ms) {
  // just call low-level send
  return send(&req, sizeof(Service::Req), 0, timeout_ms);
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

    // TODO: check len and msg.size match

    auto msg = &recv_buf_.msg;
    if (msg->__meta__.type == MsgType::MsgTypeNormal) {
      recvNormal(msg);
    } else if (msg->__meta__.type == MsgType::MsgTypeRequest) {
      recvRequest(msg);
    } else if (msg->__meta__.type == MsgType::MsgTypeResponse) {
      recvResponse(msg);
    } else {
      UROS_PRINT("Unknow msg type: %d\n", msg->__meta__.type);
    }
  }
}

inline void TransportBase::serviceWork() {
  while (true) {
    auto len = req_queue_.recv(req_buf_.data, sizeof(req_buf_), -1);
    auto &meta = service_metas_[req_buf_.msg.__meta__.id.req.service];
    if (meta->service->call(&req_buf_.msg, &rsp_buf_.msg, this, 1000)) {
      send(rsp_buf_.data, meta->service->rspSize(), -1); // TODO: timeout
    }
  }
}

inline void TransportBase::recvNormal(const MsgBase *msg) {
  UROS_PRINT("TransportBase::recvNormal, topic_id: %d\n",
             msg->__meta__.id.msg.topic);
  auto topic_id = msg->__meta__.id.msg.topic;
  if (topic_id >= topic_metas_.size() || !topic_metas_[topic_id]) {
    UROS_PRINT("Invalid msg\n");
    return;
  }

  auto &meta = topic_metas_[topic_id];

  meta->topic->write(this, msg);
}

inline void TransportBase::recvRequest(const MsgBase *msg) {
  auto service_id = msg->__meta__.id.req.service;
  if (service_id >= service_metas_.size() || !service_metas_[service_id]) {
    return;
  }

  auto &meta = service_metas_[service_id];

  if (!req_queue_.send(msg, meta->service->reqSize(), 0)) {
    UROS_PRINT("transport request queue overflow, drop request\n");
  }
}

inline void TransportBase::recvResponse(const MsgBase *msg) {
  UROS_PRINT("TransportBase::recvResponse, service_id: %d\n",
             msg->__meta__.id.rsp.service);
  auto service_id = msg->__meta__.id.rsp.service;
  if (service_id >= service_metas_.size() || !service_metas_[service_id]) {
    UROS_PRINT("Invalid response\n");
    return;
  }

  auto &meta = service_metas_[service_id];

  meta->service->writeRsp(msg, this);
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
