#pragma once

#include "etl/vector.h"

#include "uros_config.h"

#include "topic.h"
#include "transport.h"

namespace uros {

template <typename Derived>
struct TransportRemote : public TransportBase<Derived> {
  using Base = TransportBase<Derived>;

  using Base::derived;
  using Base::id_;
  using Base::req_infos_;
  using Base::topics_;
  using Base::services_;

  void initImpl() {
    send_worker_ = std::make_unique<Thread>(
        "transport_send_worker", UROS_TRANSPORT_WORKER_STACK_DEPTH,
        UROS_TRANSPORT_WORKER_PRIORITY, [&]() { sendWork(); });
    CHECK(send_worker_);

    recv_worker_ = std::make_unique<Thread>(
        "transport_recv_worker", UROS_TRANSPORT_WORKER_STACK_DEPTH,
        UROS_TRANSPORT_WORKER_PRIORITY, [&]() { recvWork(); });
    CHECK(recv_worker_);

    send_rsp_worker_ = std::make_unique<Thread>(
        "transport_send_rsp_worker", UROS_TRANSPORT_WORKER_STACK_DEPTH,
        UROS_TRANSPORT_WORKER_PRIORITY, [&]() { sendRspWork(); });
    CHECK(send_rsp_worker_);
  }

  // nonblocking!
  template <typename Topic>
  void writeImpl(Topic *topic, const typename Topic::Msg msg) {
    LockGuard<CriticalLock> lg; // MessageBuffer do not support multiple writer
    send_queue_.send(&msg, sizeof(typename Topic::Msg), 0); // non-blocking push
  }

  template <typename Service>
  bool sendRequestImpl(Service *service, const typename Service::Req &req,
                       int tsp, int timeout_ms) {
    int64_t now; // TODO: now()

    int k = -1; // TODO: lock
    for (auto i = 0u; i < req_infos_.size(); ++i) {
      if (req_infos_[i].req_id == req.id) {
        return false; // loop back req
      }
      if (req_infos_[i].deadline <= now) {
        k = i;
      }
    }

    // no slot
    if (k < 0) {
      return false;
    }

    auto &info = req_infos_[k];

    // found slot, try send req
    auto ret = send(&req, sizeof(Service::Req), timeout_ms);
    if (ret <= 0) {
      return false;
    }

    info[k]->tsp_in = tsp;
    info[k]->req_id = req.id;
    info[k]->deadline = now + timeout_ms * 1e6;

    return true;
  }

  // nonblocking!
  template <typename Service>
  bool sendResponseImpl(Service *service, const typename Service::Rsp &rsp) {
    LockGuard<CriticalLock> lg; // MessageBuffer do not support multiple writer
    return send_rsp_queue_.send(&rsp, sizeof(rsp), 0); // non-blocking push
  }

  // blocking send, thread safe
  int send(const void *data, size_t len, int timeout_ms = -1) {
    return derived().sendImpl(data, len, timeout_ms);
  }

  // blocking recv
  int recv(void *data, size_t len, int timeout_ms = -1) {
    return derived().recvImpl(data, len, timeout_ms);
  }

protected:
  void sendWork() {
    while (true) {
      // TODO: MessageBuffer requires two copies, make it only once
      auto len = send_queue_.recv(send_buf_.data, sizeof(send_buf_), -1);
      send(send_buf_.data, len);
    }
  }

  void sendRspWork() {
    while (true) {
      // TODO: MessageBuffer requires two copies, make it only once
      auto len =
          send_rsp_queue_.recv(send_rsp_buf_.data, sizeof(send_rsp_buf_), -1);
      send(send_rsp_buf_.data, len);
    }
  }

  void recvWork() {
    while (true) {
      auto len = recv(recv_buf_.data, sizeof(recv_buf_)); // block recv
      if (len < sizeof(uint32_t)) {
        continue;
      }

      int64_t now;

      if (recv_buf_.msg.__id__.type == MsgTypeNormal) { // if is normal msg
        auto topic_id = recv_buf_.msg.__id__.topic_service;
        if (topic_id >= topics_.size() || topics_[topic_id] == nullptr ||
            topics_[topic_id]->msgSize() != len) {
          continue;
        }
        // topic's recv should be non-blocking
        topics_[topic_id]->recv(id_, &recv_buf_.msg);
      } else if (recv_buf_.msg.__id__.type == MsgTypeRequest) {
      } else if (recv_buf_.msg.__id__.type == MsgTypeResponse) {
        auto service_id = recv_buf_.msg.__id__.topic_service;

        int tsp_id = -1;
        for (auto &info : req_infos_) {
          if (info.req_id == reinterpret_cast<uint32_t>(recv_buf_.msg.__id__) &&
              info.deadline <= now) {
            tsp_id = info.tsp_in;
            info.deadline = -1;
          }
        }

        if(tsp_id>0){
          services_[service_id]->routeResponse(id_, tsp_id, &recv_buf_.msg);
        }

        // 1. find if the crosspondence req exists
        // 2. unregister the req, find the route out
        // 3. user service to route rsp
      }
    }
  }

protected:
  union Buffer {
    MsgBase msg;
    uint8_t data[UROS_MSG_MAX_SIZE];
  };

  std::unique_ptr<Thread> send_worker_;
  MessageBuffer send_queue_{2 * UROS_MSG_MAX_SIZE};
  Buffer send_buf_;

  std::unique_ptr<Thread> recv_worker_;
  Buffer recv_buf_;

  std::unique_ptr<Thread> send_rsp_worker_;
  MessageBuffer send_rsp_queue_{4 * UROS_RSP_MAX_SIZE};
  Buffer send_rsp_buf_;
};

} // namespace uros
