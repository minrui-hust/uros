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
  using Base::topic_metas_;

  void initImpl() {
    send_worker_ = std::make_unique<Thread>(
        "transport_send_worker", UROS_TRANSPORT_WORKER_STACK_DEPTH,
        UROS_TRANSPORT_WORKER_PRIORITY, [&]() { sendWork(); });
    CHECK(send_worker_);

    recv_worker_ = std::make_unique<Thread>(
        "transport_recv_worker", UROS_TRANSPORT_WORKER_STACK_DEPTH,
        UROS_TRANSPORT_WORKER_PRIORITY, [&]() { recvWork(); });
    CHECK(recv_worker_);
  }

  template <typename Topic>
  void writeImpl(Topic *topic, const typename Topic::Msg msg) {
    msg.topic_id = topic->id();

    LockGuard<CriticalLock> lg; // MessageBuffer do not support multiple writer
    send_queue_.send(&msg, sizeof(typename Topic::Msg), 0); // non-blocking push
  }

  // blocking send, thread safe is NOT required
  int send(const void *data, size_t len) {
    return derived().sendImpl(data, len);
  }

  // blocking recv, thread safe is NOT required
  int recv(void *data, size_t len) { return derived().recvImpl(data, len); }

protected:
  void sendWork() {
    while (true) {
      // TODO: MessageBuffer requires two copies, make it only once
      auto len = send_queue_.recv(send_buf_.data, sizeof(send_buf_), -1);
      send(send_buf_.data, len);
    }
  }

  void recvWork() {
    while (true) {
      auto len = recv(recv_buf_.data, sizeof(recv_buf_)); // block recv
      auto topic_id = recv_buf_.msg.topic_id;
      if (topic_id >= topic_metas_.size() ||
          topic_metas_[topic_id].topic == nullptr ||
          topic_metas_[topic_id].topic->msgSize() != len) {
        continue;
      }
      // topic's recv should be non-blocking
      topic_metas_[topic_id].topic->recv(id_, &recv_buf_.msg);
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
};

} // namespace uros
