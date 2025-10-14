#pragma once

#include "transport_remote.h"

namespace uros {

inline void TransportRemote::init() {
  send_worker_ = std::make_unique<Thread>(
      "transport_send_worker", UROS_TRANSPORT_WORKER_STACK_DEPTH,
      UROS_TRANSPORT_WORKER_PRIORITY, [&]() { sendWork(); });
  CHECK(send_worker_);

  recv_worker_ = std::make_unique<Thread>(
      "transport_recv_worker", UROS_TRANSPORT_WORKER_STACK_DEPTH,
      UROS_TRANSPORT_WORKER_PRIORITY, [&]() { recvWork(); });
  CHECK(recv_worker_);
}

inline bool TransportRemote::putNormal(const MsgBase *msg, int from_tsp,
                                       int timeout_ms) {
  UROS_PRINT("TransportRemote.putNormal begin msg: %d, %d\n", msg->__id__.entry,
             msg->__id__.seq);

  auto topic_id = msg->__id__.entry;
  if (topic_id >= topic_metas_.size()) {
    return false;
  }

  auto &meta = topic_metas_[topic_id];
  if (!meta.topic) {
    return false;
  }

  bool pended = false;
  {
    LockGuard<CriticalLock> lg;
    bool full = (meta.wr - meta.rd) >= 2;
    bool empty = meta.wr == meta.rd;

    uint8_t diff = msg->__id__.seq - meta.msgs[(meta.wr - 1) & 1];
    bool drop = !empty && (diff > 0 && diff < 128);

    if (!full && !drop) {
      memcpy(meta.msgs[(meta.wr++) & 1].get(), msg, meta.topic->msgSize());
      pended = true;
    }
  }

  if (pended) {
    ThreadNotify(send_worker_.get(), 1 << topic_id);
  }

  UROS_PRINT("TransportRemote.putNormal done msg: %d, %d\n", msg->__id__.entry,
             msg->__id__.seq);

  return true;
}

inline bool TransportRemote::putRequest(const MsgBase *msg, int from_tsp,
                                        int timeout_ms) {
  return false; // TODO
}

inline bool TransportRemote::putResponse(const MsgBase *msg, int from_tsp,
                                         int timeout_ms) {
  return false; // TODO
}

inline bool TransportRemote::putServiceBroadcast(const MsgBase *msg,
                                                 int from_tsp, int timeout_ms) {
  return false; // TODO
}

inline bool TransportRemote::putServiceDiscovery(const MsgBase *msg,
                                                 int from_tsp, int timeout_ms) {
  return false; // TODO
}

inline void TransportRemote::recvNormal(const MsgBase *msg) {
  router_->route(msg, id_, -1, 0); // broadcast without timeout
}

inline void TransportRemote::recvRequest(const MsgBase *msg) {
  // TODO
}

inline void TransportRemote::recvResponse(const MsgBase *msg) {
  // TODO
}

inline void TransportRemote::recvServiceBroadcast(const MsgBase *msg) {
  // TODO
}

inline void TransportRemote::recvServiceDiscovery(const MsgBase *msg) {
  // TODO
}

inline void TransportRemote::sendWork() {
  uint32_t flags;
  while (true) {
    ThreadNotifyWait(0, topic_bit_mask_, &flags, -1); // blocking wait
    UROS_PRINT("TransportRemote::sendWork: wait done, 0x%x\n", flags);

    for (auto i = 0u; i < topic_metas_.size(); ++i) {
      auto &meta = topic_metas_[i];
      if (flags & (1 << i)) {
        bool empty;
        int rd;
        { // get empty state and read ptr
          LockGuard<CriticalLock> lg;
          empty = meta.rd == meta.wr;
          rd = meta.rd = meta.wr - 1; // only keep the latest data
        }

        // blocking send
        send(meta.msgs[rd & 1].get(), meta.topic->msgSize(), meta.topic->prio(),
             -1);

        { // update read ptr
          LockGuard<CriticalLock> lg;
          ++meta.rd;
        }
      }
    }
  }
}

inline void TransportRemote::recvWork() {
  int prio;
  while (true) {
    auto len = recv(recv_buf_.data, sizeof(recv_buf_), &prio, -1); // block recv
    if (len < sizeof(MsgId)) {
      continue;
    }

    auto msg = &recv_buf_.msg;
    if (msg->__id__.type == MsgTypeNormal) {
      recvNormal(msg);
    } else if (msg->__id__.type == MsgTypeRequest) {
      recvRequest(msg);
    } else if (msg->__id__.type == MsgTypeResponse) {
      recvResponse(msg);
    } else if (msg->__id__.type == MsgTypeServiceBroadcast) {
      recvServiceBroadcast(msg);
    } else if (msg->__id__.type == MsgTypeServiceDiscovery) {
      recvServiceDiscovery(msg);
    } else {
      UROS_PRINT("Unknow msg type: %d\n", msg->__id__.type);
    }
  }
}

} // namespace uros
