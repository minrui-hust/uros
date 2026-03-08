#pragma once

#include "service.h"
#include "system.h"
#include "topic.h"
#include "transport.h"

// ============================================================
// transport.hpp — TransportBase / TransportManager 实现
// ============================================================

namespace uros {

// declareTopic(): 在本传输层上声明一个话题。
// 声明后：
//   - 在 topic_metas_ 中分配一个条目（key = topic hash ID）
//   - 为该话题分配发送通知位（mask），用于 sendWork() 区分哪个话题有更新
//   - 调用 Topic::registerTransport() 建立反向关联（Topic 也持有本 Transport）
inline bool TransportBase::declareTopic(const char *topic_name) {
  auto topic = TopicManager::FindTopic<TopicBase>(topic_name);
  CHECK(topic);

  auto topic_id = topic->id();

  if (topic_metas_.contains(topic_id)) {
    UROS_PRINT("topic '%s' already declared on transport %d \n", topic_name,
               id_);
    return false;
  }

  auto [iter, ret] = topic_metas_.insert(
      {topic_id, etl::unique_ptr<TopicMeta>(new TopicMeta)});
  CHECK(ret);

  iter->second->mask = 1 << (topic_metas_.size() - 1); // 分配第 N 个事件位
  iter->second->topic = topic;
  topic->registerTransport(this);
  topic_bit_mask_ |= iter->second->mask;

  UROS_PRINT("add topic '%s' to transport %d succeed\n", topic_name, id_);

  return true;
}

// declareService(): 在本传输层上声明一个服务。
// 声明后，Transport 能够接收和转发该服务的请求/应答/广播消息。
inline bool TransportBase::declareService(const char *service_name) {
  auto service = ServiceManager::FindService<ServiceBase>(service_name);
  if (!service) {
    return false;
  }

  auto service_id = service->id();
  if (service_metas_.contains(service_id)) {
    UROS_PRINT("service '%s' already declared on transport %d \n", service_name,
               id_);
    return false;
  }

  auto [iter, ret] = service_metas_.insert(
      {service_id, etl::unique_ptr<ServiceMeta>(new ServiceMeta)});
  CHECK(ret);

  iter->second->service = service;
  service->registerTransport(this);

  UROS_PRINT("add service '%s' to transport %d succeed\n", service_name, id_);

  return true;
}

// init(): 启动三个工作线程
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

// notify(): 由 TopicT::notify() 调用，向发送线程发送 ThreadNotify 信号。
// flags 中包含该话题对应的事件位，sendWork() 通过位掩码区分哪些话题有更新。
template <typename Topic> void TransportBase::notify(Topic *topic) {
  ThreadNotify(send_worker_.get(), topic_metas_[topic->id()]->mask);
}

// sendReq(): 将服务请求直接通过底层 send() 发出（不经过发送线程队列）。
// 在发送前填写 sys_pre = 本节点 ID，以便对端能正确记录前一跳。
template <typename Service>
bool TransportBase::sendReq(Service *service, const typename Service::Req &req,
                            int timeout_ms) {
  req.timeout = timeout_ms; // NOTE: here we do not count the send delay
  req.__meta__.sys_pre = System::Id();
  return send(&req, sizeof(req), 0, timeout_ms);
}

// sendServiceAnnounce(): 将服务广播通过底层 send() 发出（最高优先级 = -1 的信道实现自定义）
template <typename Service>
bool TransportBase::sendServiceAnnounce(Service *service, const MsgBase &sbc,
                                        int timeout_ms) {
  return send(&sbc, sizeof(sbc), 0, timeout_ms);
}

// sendWork(): 话题发送线程主循环。
// 使用 ThreadNotifyWait() 等待任意话题的更新通知（flags 为位掩码）。
// 对每个置位的话题，读取最新消息（版本比较）并调用 send() 发出。
// 若读取返回 0（无新数据），则不发送（避免重复发送）。
inline void TransportBase::sendWork() {
  uint32_t flags;
  while (true) {
    ThreadNotifyWait(0, topic_bit_mask_, &flags, -1); // blocking wait
    UROS_PRINT("TransportBase::sendWork: wait done, 0x%x\n", flags);

    for (auto &[_, meta] : topic_metas_) {
      if (flags & meta->mask) {
        auto len = meta->topic->read(&send_buf_.msg, meta->seq);
        if (len > 0) {
          send(&send_buf_.msg, len, meta->topic->prio(), -1);
        }
      }
    }
  }
}

// recvWork(): 消息接收线程主循环。
// 阻塞等待底层 recv()，收到消息后根据 MsgMeta::type 分发：
//   - MsgTypeNormal           → recvNormal()（话题消息）
//   - MsgTypeServiceBroadcast → recvServiceBroadcast()（服务广播）
//   - MsgTypeRequest          → 检查 sys_nxt，匹配则 recvRequest()，否则丢弃
//   - MsgTypeResponse         → 检查 sys_nxt，匹配则 recvResponse()，否则丢弃
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
      if (msg->__meta__.sys_nxt ==
          System::Id()) { // drop broadcast req not belong to this system
        recvRequest(msg, len);
      } else {
        UROS_PRINT("drop req not blong to sys '%d'\n", System::Id());
      }
    } else if (msg->__meta__.type == MsgType::MsgTypeResponse) {
      if (msg->__meta__.sys_nxt ==
          System::Id()) { // drop broadcast req not belong to this system
        recvResponse(msg, len);
      } else {
        const auto &rsp = *static_cast<ReqBase *>(msg);
        UROS_PRINT("drop rsp not blong to sys '%d':type(%d), sys_src(%d), "
                   "sys_dst(%d), sys_pre(%d), sys_nxt(%d), service(0x%x), "
                   "client(%d), seq(%d)\n",
                   System::Id(), rsp.__meta__.type, rsp.__meta__.sys_src,
                   rsp.__meta__.sys_dst, rsp.__meta__.sys_pre,
                   rsp.__meta__.sys_nxt, rsp.__meta__.entry_hash, rsp.client,
                   rsp.seq);
      }
    } else {
      UROS_PRINT("Unknow msg type: %d\n", msg->__meta__.type);
    }
  }
}

// serviceWork(): 服务请求处理线程主循环。
// 阻塞等待 req_queue_（由 recvRequest() 入队），取出后：
//   1. 查找对应的 Service
//   2. 调用 Service::call()（可能再次路由到本地或更远的节点）
//   3. 若成功，填写应答的路由字段（sys_pre, sys_nxt），然后通过 send() 发回
inline void TransportBase::serviceWork() {
  while (true) {
    auto len = req_queue_.recv(req_buf_.data, sizeof(req_buf_), -1);
    auto service = service_metas_[req_buf_.msg.__meta__.entry_hash]->service;
    if (service->call(this, &req_buf_.msg, &rsp_buf_.msg,
                      req_buf_.msg.timeout)) {
      // set sys_from and sys_to, so we can route back rsp to where req from
      rsp_buf_.msg.__meta__.sys_pre = System::Id();
      rsp_buf_.msg.__meta__.sys_nxt = req_buf_.msg.__meta__.sys_pre;

      UROS_PRINT("remote call request done with rsp: type(%d), sys_src(%d), "
                 "sys_dst(%d), "
                 "sys_pre(%d), "
                 "sys_nxt(%d), service(0x%x), "
                 "client(%d), seq(%d)\n",
                 rsp_buf_.msg.__meta__.type, rsp_buf_.msg.__meta__.sys_src,
                 rsp_buf_.msg.__meta__.sys_dst, rsp_buf_.msg.__meta__.sys_pre,
                 rsp_buf_.msg.__meta__.sys_nxt,
                 rsp_buf_.msg.__meta__.entry_hash, rsp_buf_.msg.client,
                 rsp_buf_.msg.seq);

      send(rsp_buf_.data, service->rspSize(), -1);
    }
  }
}

// recvNormal(): 处理收到的普通话题消息。
// 按 entry_hash 查找 Topic，校验长度后写入 Topic（触发本地订阅者通知）。
inline void TransportBase::recvNormal(const MsgBase *msg, size_t len) {
  auto topic_id = msg->__meta__.entry_hash;
  auto iter = topic_metas_.find(topic_id);
  if (iter == topic_metas_.end()) {
    UROS_PRINT("Invalid msg: topic not found\n");
    return;
  }
  
  // For variable-length messages, check if the actual length matches the metadata
  size_t expected_len = msg->__meta__.len + sizeof(MsgBase);
  if (expected_len != len) {
    UROS_PRINT("Invalid msg: len mismatch (received=%zu, expected=%zu)\n", len, expected_len);
    return;
  }
  
  // Check if the length is within the valid range
  if (len < sizeof(MsgBase) || len > iter->second->topic->msgSize()) {
    UROS_PRINT("Invalid msg: len out of range (len=%zu, max=%zu)\n", len, iter->second->topic->msgSize());
    return;
  }

  UROS_PRINT("TransportBase::recvNormal, topic_id: 0x%x\n", topic_id);

  iter->second->topic->write(this, msg);
}

// recvRequest(): 处理收到的服务请求消息。
// 按 entry_hash 查找 Service，校验请求长度后将请求放入 req_queue_。
// serviceWork() 线程从队列中取出请求并异步处理（解耦接收和业务处理）。
inline void TransportBase::recvRequest(const MsgBase *msg, size_t len) {
  auto service_id = msg->__meta__.entry_hash;
  auto iter = service_metas_.find(service_id);
  if (iter == service_metas_.end() || iter->second->service->reqSize() != len) {
    UROS_PRINT("Invalid req\n");
    return;
  }

  UROS_PRINT("TransportBase::recvRequest, service_id: 0x%x\n", service_id);

  auto &meta = iter->second;

  if (!req_queue_.send(msg, meta->service->reqSize(), 0)) {
    UROS_PRINT("transport request queue overflow, drop request\n");
  }
}

// recvResponse(): 处理收到的服务应答消息。
// 按 entry_hash 查找 Service，校验应答长度后调用 Service::writeRsp()，
// 唤醒等待应答的 Client（通过 sem_rsp_ 信号量）。
inline void TransportBase::recvResponse(const MsgBase *msg, size_t len) {
  auto service_id = msg->__meta__.entry_hash;
  auto iter = service_metas_.find(service_id);
  if (iter == service_metas_.end() || iter->second->service->rspSize() != len) {
    UROS_PRINT("Invalid rsp\n");
    return;
  }

  UROS_PRINT("TransportBase::recvResponse, service_id: 0x%x\n", service_id);

  auto &meta = iter->second;

  meta->service->writeRsp(this, msg);
}

// recvServiceBroadcast(): 处理收到的服务广播消息。
// 按 entry_hash 查找 Service，递增 dist（每经过一跳 +1，上限 INT16_MAX），
// 然后调用 Service::writeServiceBroadcast() 更新路由并继续转发。
inline void TransportBase::recvServiceBroadcast(const MsgBase *msg,
                                                size_t len) {
  auto service_id = msg->__meta__.entry_hash;
  auto iter = service_metas_.find(service_id);
  if (iter == service_metas_.end() || sizeof(MsgBase) != len) {
    UROS_PRINT("Invalid sbc\n");
    return;
  }

  UROS_PRINT("TransportBase::recvServiceBroadcast, service_id: 0x%x\n",
             service_id);

  auto &meta = iter->second;

  // increate the sbc dist
  auto sbc = static_cast<const ServiceBroadcast *>(msg);
  if (sbc->dist < UROS_SBC_DIST_MAX) {
    ++sbc->dist;
  }

  // write sbc to service
  meta->service->writeServiceBroadcast(this, msg);
}

// addTransport(): 创建新传输层实例，分配 ID（= 当前列表长度），加入全局列表
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

// init(): 依次调用所有已注册传输层的 init()，启动其工作线程
inline void TransportManager::init() {
  for (auto &tsp : tsps_) {
    tsp->init();
  }
}

} // namespace uros
