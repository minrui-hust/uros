#pragma once

#include <cstddef>
#include <cstdint>

#include "etl/vector.h"

#include "msg.h"
#include "platform.h"

namespace uros {

struct TopicBase;
struct ServiceBase;

struct TopicMeta {
  TopicBase *topic = nullptr;
  int16_t seq = -1;
};

struct ServiceMeta {
  ServiceBase *service = nullptr;
  // TODO: maybe more
};

struct TransportBase {

  auto &id() { return id_; }
  const auto &id() const { return id_; }

  bool declareTopic(const char *topic_name);

  bool declareService(const char *service_name);

  void init();

  template <typename Topic> void notify(Topic *topic);

  template <typename Service>
  bool sendReq(Service *service, const typename Service::Req &req,
               int timeout_ms);

  virtual ~TransportBase() = default;

protected:
  void sendWork();

  void recvWork();

  void serviceWork();

  void recvNormal(const MsgBase *msg);
  void recvRequest(const MsgBase *msg);
  void recvResponse(const MsgBase *msg);

  // thread safe
  virtual int send(const void *data, size_t len, int prio,
                   int timeout_ms = -1) = 0;

  // thread safe
  virtual int recv(void *data, size_t len, int *prio = nullptr,
                   int timeout_ms = -1) = 0;

protected:
  int32_t id_ = -1;
  uint32_t topic_bit_mask_ = 0;

  etl::array<etl::unique_ptr<TopicMeta>, UROS_MAX_TOPICS> topic_metas_{};
  etl::array<etl::unique_ptr<ServiceMeta>, UROS_MAX_SERVICES> service_metas_{};

  std::unique_ptr<Thread> send_worker_;
  std::unique_ptr<Thread> recv_worker_;
  std::unique_ptr<Thread> service_worker_;

  union Buffer {
    MsgBase msg;
    uint8_t data[UROS_MSG_MAX_SIZE];
  };
  Buffer send_buf_;
  Buffer recv_buf_;

  MessageBuffer req_queue_{UROS_TRANSPORT_REQ_QUEUE_SIZE};
  Buffer req_buf_;
  Buffer rsp_buf_;
};

struct TransportManager {
  template <typename Transport> static Transport *AddTransport() {
    return Instance().addTransport<Transport>();
  }

  static void Init() { return Instance().init(); }

protected:
  static TransportManager &Instance() {
    static TransportManager inst;
    return inst;
  }

  template <typename Transport> Transport *addTransport();

  void init();

protected:
  TransportManager() = default;
  TransportManager(const TransportManager &other) = delete;
  TransportManager &operator=(const TransportManager &other) = delete;

protected:
  etl::vector<etl::unique_ptr<TransportBase>, UROS_MAX_TRANSPORTS> tsps_;
};

} // namespace uros
