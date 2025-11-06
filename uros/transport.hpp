#pragma once

#include "service.h"
#include "system.h"
#include "topic.h"
#include "transport.h"

namespace uros {

inline bool TransportBase::declareTopic(const char *topic_name) {
  auto topic = TopicManager::FindTopic<TopicBase>(topic_name);
  if (!topic) {
    return false;
  }

  auto topic_id = topic->id();
  CHECK(topic_id < 256);

  if (topic_metas_.contains(topic_id)) {
    UROS_PRINT("topic '%s' already declared on transport %d \n", topic_name, id_);
    return false;
  }

  auto [iter, ret] = topic_metas_.insert({topic_id, etl::unique_ptr<TopicMeta>(new TopicMeta)});
  CHECK(ret);

  iter->second->topic = topic;
  iter->second->mask = 1 << (topic_metas_.size() - 1);
  topic->registerTransport(this);

  topic_bit_mask_ |= iter->second->mask;

  UROS_PRINT("add topic '%s' to transport %d succeed\n", topic_name, id_);

  return true;
}

inline bool TransportBase::declareService(const char *service_name) {
  auto service = ServiceManager::FindService<ServiceBase>(service_name);
  if (!service) {
    return false;
  }

  auto service_id = service->id();
  if (service_metas_.contains(service_id)) {
    UROS_PRINT("service '%s' already declared on transport %d \n", service_name, id_);
    return false;
  }

  auto [iter, ret] = service_metas_.insert({service_id, etl::unique_ptr<ServiceMeta>(new ServiceMeta)});
  CHECK(ret);

  iter->second->service = service;
  service->registerTransport(this);

  UROS_PRINT("add service '%s' to transport %d succeed\n", service_name, id_);

  return true;
}

inline void TransportBase::init() {
  send_worker_ = std::make_unique<Thread>("tsp_send_worker", UROS_TRANSPORT_WORKER_STACK_DEPTH,
                                          UROS_TRANSPORT_WORKER_PRIORITY, [&]() { sendWork(); });
  CHECK(send_worker_);

  recv_worker_ = std::make_unique<Thread>("tsp_recv_worker", UROS_TRANSPORT_WORKER_STACK_DEPTH,
                                          UROS_TRANSPORT_WORKER_PRIORITY, [&]() { recvWork(); });
  CHECK(recv_worker_);

  service_worker_ = std::make_unique<Thread>("tsp_service_worker", UROS_TRANSPORT_WORKER_STACK_DEPTH,
                                             UROS_TRANSPORT_WORKER_PRIORITY, [&]() { serviceWork(); });
  CHECK(service_worker_);
}

template <typename Topic>
void TransportBase::notify(Topic *topic) {
  ThreadNotify(send_worker_.get(), 1 << topic->id());
}

template <typename Service>
bool TransportBase::sendReq(Service *service, const typename Service::Req &req, int timeout_ms) {
  req.timeout = timeout_ms;  // NOTE: here we do not count the send delay
  req.__meta__.id.req.sys_from = System::Id();
  return send(&req, sizeof(req), 0, timeout_ms);
}

template <typename Service>
bool TransportBase::sendServiceAnnounce(Service *service, const MsgBase &sbc, int timeout_ms) {
  return send(&sbc, sizeof(sbc), 0, timeout_ms);
}

inline void TransportBase::sendWork() {
  uint32_t flags;
  while (true) {
    ThreadNotifyWait(0, topic_bit_mask_, &flags, -1);  // blocking wait
    UROS_PRINT("TransportBase::sendWork: wait done, 0x%x\n", flags);

    for (auto &[_, meta] : topic_metas_) {
      if (flags & (1 << meta->mask)) {
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
      recvNormal(msg, len);
    } else if (msg->__meta__.type == MsgType::MsgTypeServiceBroadcast) {
      recvServiceBroadcast(msg, len);
    } else if (msg->__meta__.type == MsgType::MsgTypeRequest) {
      if (msg->__meta__.id.req.sys_to == System::Id()) {  // drop broadcast req not belong to this system
        recvRequest(msg, len);
      } else {
        UROS_PRINT("drop req not blong to sys '%d'\n", System::Id());
      }
    } else if (msg->__meta__.type == MsgType::MsgTypeResponse) {
      if (msg->__meta__.id.rsp.sys_to == System::Id()) {  // drop broadcast req not belong to this system
        recvResponse(msg, len);
      } else {
        const auto &rsp = *static_cast<ReqBase *>(msg);
        UROS_PRINT(
            "drop rsp not blong to sys '%d':type(%d), sys(%d), "
            "sys_from(%d), sys_to(%d), service(%d), client(%d), seq(%d)\n",
            System::Id(), rsp.__meta__.type, rsp.__meta__.sys, rsp.__meta__.id.rsp.sys_from, rsp.__meta__.id.rsp.sys_to,
            rsp.__meta__.id.rsp.service, rsp.__meta__.id.rsp.client, rsp.__meta__.id.rsp.seq);
      }
    } else {
      UROS_PRINT("Unknow msg type: %d\n", msg->__meta__.type);
    }
  }
}

inline void TransportBase::serviceWork() {
  while (true) {
    auto len = req_queue_.recv(req_buf_.data, sizeof(req_buf_), -1);
    auto service = service_metas_[req_buf_.msg.__meta__.id.req.service]->service;
    if (service->call(this, &req_buf_.msg, &rsp_buf_.msg, req_buf_.msg.timeout)) {
      // set sys_from and sys_to, so we can route back rsp to where req from
      rsp_buf_.msg.__meta__.id.rsp.sys_from = System::Id();
      rsp_buf_.msg.__meta__.id.rsp.sys_to = req_buf_.msg.__meta__.id.req.sys_from;

      UROS_PRINT(
          "remote call request done with rsp: type(%d), sys(%d), "
          "sys_from(%d), "
          "sys_to(%d), service(%d), "
          "client(%d), seq(%d)\n",
          rsp_buf_.msg.__meta__.type, rsp_buf_.msg.__meta__.sys, rsp_buf_.msg.__meta__.id.rsp.sys_from,
          rsp_buf_.msg.__meta__.id.rsp.sys_to, rsp_buf_.msg.__meta__.id.rsp.service,
          rsp_buf_.msg.__meta__.id.rsp.client, rsp_buf_.msg.__meta__.id.rsp.seq);

      send(rsp_buf_.data, service->rspSize(), -1);
    }
  }
}

inline void TransportBase::recvNormal(const MsgBase *msg, size_t len) {
  auto topic_id = msg->__meta__.id.msg.topic;
  auto iter = topic_metas_.find(topic_id);
  if (iter == topic_metas_.end() || iter->second->topic->msgSize() != len) {
    UROS_PRINT("Invalid msg\n");
    return;
  }

  UROS_PRINT("TransportBase::recvNormal, topic_id: %d\n", topic_id);

  iter->second->topic->write(this, msg);
}

inline void TransportBase::recvRequest(const MsgBase *msg, size_t len) {
  auto service_id = msg->__meta__.id.req.service;
  auto iter = service_metas_.find(service_id);
  if (iter == service_metas_.end() || iter->second->service->reqSize() != len) {
    UROS_PRINT("Invalid req\n");
    return;
  }

  UROS_PRINT("TransportBase::recvRequest, service_id: %d\n", service_id);

  auto &meta = iter->second;

  if (!req_queue_.send(msg, meta->service->reqSize(), 0)) {
    UROS_PRINT("transport request queue overflow, drop request\n");
  }
}

inline void TransportBase::recvResponse(const MsgBase *msg, size_t len) {
  auto service_id = msg->__meta__.id.rsp.service;
  auto iter = service_metas_.find(service_id);
  if (iter == service_metas_.end() || iter->second->service->rspSize() != len) {
    UROS_PRINT("Invalid rsp\n");
    return;
  }

  UROS_PRINT("TransportBase::recvResponse, service_id: %d\n", service_id);

  auto &meta = iter->second;

  meta->service->writeRsp(this, msg);
}

inline void TransportBase::recvServiceBroadcast(const MsgBase *msg, size_t len) {
  auto service_id = msg->__meta__.id.sbc.service;
  auto iter = service_metas_.find(service_id);
  if (iter == service_metas_.end() || sizeof(MsgBase) != len) {
    UROS_PRINT("Invalid sbc\n");
    return;
  }

  UROS_PRINT("TransportBase::recvServiceBroadcast, service_id: %d\n", service_id);

  auto &meta = iter->second;

  // increate the sbc dist
  if (msg->__meta__.id.sbc.dist < UROS_SBC_DIST_MAX) {
    ++msg->__meta__.id.sbc.dist;
  }

  // write sbc to service
  meta->service->writeServiceBroadcast(this, msg);
}

template <typename Transport>
Transport *TransportManager::addTransport() {
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

}  // namespace uros
