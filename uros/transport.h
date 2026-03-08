#pragma once

#include <cstddef>
#include <cstdint>

#include "etl/unordered_map.h"
#include "etl/vector.h"

#include "msg.h"
#include "platform.h"

// ============================================================
// transport.h — 传输层（Transport）定义
//
// Transport 是 uros 中间件与底层物理/逻辑通信信道（UART、CAN、IPC、
// UDP 组播等）之间的抽象层。每个 Transport 实例对应一条信道。
//
// 核心职责：
//   1. 话题转发（pub/sub 跨节点）：
//      - 话题有新消息时，发送线程（sendWork）被唤醒并将消息序列化发出。
//      - 收到远端话题消息时，接收线程（recvWork）将消息写入本地 Topic。
//   2. 服务转发（request/response 跨节点）：
//      - 收到请求时，接收线程将请求放入 req_queue_，
//        服务线程（serviceWork）从队列中取出并调用 Service::call()，
//        然后将应答发回。
//      - 收到应答时，直接调用 Service::writeRsp() 唤醒等待的 Client。
//   3. 服务广播转发（ServiceBroadcast）：
//      - 收到广播时，调用 Service::writeServiceBroadcast() 更新路由距离，
//        并在其他传输层上转发。
//
// 线程模型：
//   每个 Transport 启动三个独立线程：
//     send_worker_    — 话题发送线程（被 ThreadNotify 唤醒）
//     recv_worker_    — 消息接收线程（阻塞等待底层 recv()）
//     service_worker_ — 服务请求处理线程（阻塞等待 req_queue_）
//
// 子类化：
//   具体信道通过继承 TransportBase 并实现 send()/recv() 两个纯虚函数来接入。
//   TransportManager 统一管理所有 Transport 实例。
//
// 话题声明（declareTopic）/ 服务声明（declareService）：
//   每个 Transport 上需要显式声明要转发的话题和服务，
//   声明后才会建立 Transport ↔ Topic/Service 的双向关联。
// ============================================================

namespace uros {

struct TopicBase;
struct ServiceBase;

// TopicMeta: Transport 对某个话题的本地记录
struct TopicMeta {
  TopicBase *topic = nullptr; // 对应的 Topic 指针
  int seq = -1;               // 上次发送的消息序号（用于避免重复发送）
  uint32_t mask = 0;          // 该话题在发送线程通知中的事件位掩码
};

// ServiceMeta: Transport 对某个服务的本地记录（当前仅记录 Service 指针）
struct ServiceMeta {
  ServiceBase *service = nullptr;
  // TODO: maybe more
};

// TransportBase: 所有具体传输层的基类
struct TransportBase {
  // 访问器
  auto &id() { return id_; }
  const auto &id() const { return id_; }

  // 在本传输层上声明一个话题（Topic 必须已在 TopicManager 注册）
  bool declareTopic(const char *topic_name);
  // 在本传输层上声明一个服务（Service 必须已在 ServiceManager 注册）
  bool declareService(const char *service_name);

  // 启动三个工作线程（send/recv/service）
  void init();

  // 话题有新消息时，由 TopicT::notify() 调用，通知发送线程
  template <typename Topic> void notify(Topic *topic);

  // 向远端发送服务请求
  template <typename Service>
  bool sendReq(Service *service, const typename Service::Req &req,
               int timeout_ms);

  // 向远端发送服务广播
  template <typename Service>
  bool sendServiceAnnounce(Service *service, const MsgBase &sbc,
                           int timeout_ms);

  virtual ~TransportBase() = default;

protected:
  // 话题发送线程主循环：等待通知 → 读最新消息 → 调用 send()
  void sendWork();

  // 消息接收线程主循环：调用 recv() → 根据消息类型分发
  void recvWork();

  // 服务请求处理线程主循环：从 req_queue_ 取请求 → Service::call() → 发送应答
  void serviceWork();

  // 分发函数：按消息类型调用对应处理逻辑
  void recvNormal(const MsgBase *msg, size_t len);           // 普通话题消息
  void recvRequest(const MsgBase *msg, size_t len);          // 服务请求
  void recvResponse(const MsgBase *msg, size_t len);         // 服务应答
  void recvServiceBroadcast(const MsgBase *msg, size_t len); // 服务广播

  // 底层发送接口（线程安全），由子类实现
  // prio: 消息优先级；timeout_ms: 超时（-1 表示永远等待）
  virtual int send(const void *data, size_t len, int prio,
                   int timeout_ms = -1) = 0;

  // 底层接收接口（线程安全），由子类实现
  // 返回实际接收字节数，-1 表示错误；prio 可为 nullptr
  virtual int recv(void *data, size_t len, int *prio = nullptr,
                   int timeout_ms = -1) = 0;

protected:
  uint32_t id_ = -1;           // 本传输层的全局唯一 ID（由 TransportManager 分配）
  uint32_t topic_bit_mask_ = 0; // 所有已声明话题的事件位掩码 OR 值

  // 已声明的话题元数据（key = topic hash ID）
  using TopicMetaMap =
      etl::unordered_map<uint32_t, etl::unique_ptr<TopicMeta>, UROS_MAX_TOPICS>;
  TopicMetaMap topic_metas_;

  // 已声明的服务元数据（key = service hash ID）
  using ServiceMetaMap =
      etl::unordered_map<uint32_t, etl::unique_ptr<ServiceMeta>,
                         UROS_MAX_SERVICES>;
  ServiceMetaMap service_metas_;

  std::unique_ptr<Thread> send_worker_;    // 话题发送线程
  std::unique_ptr<Thread> recv_worker_;    // 消息接收线程
  std::unique_ptr<Thread> service_worker_; // 服务请求处理线程

  // 发送/接收缓冲（union 以节省内存，同一时刻只使用其中一个视图）
  union MsgBuffer {
    MsgBase msg;
    uint8_t data[UROS_MSG_MAX_SIZE];
  };
  MsgBuffer send_buf_;
  MsgBuffer recv_buf_;

  union ReqBuffer {
    ReqBase msg;
    uint8_t data[UROS_MSG_MAX_SIZE];
  };
  ReqBuffer req_buf_;

  union RspBuffer {
    RspBase msg;
    uint8_t data[UROS_MSG_MAX_SIZE];
  };
  RspBuffer rsp_buf_;

  // 服务请求队列：recvWork() 入队，serviceWork() 出队，解耦接收和处理
  MessageBuffer req_queue_{UROS_TRANSPORT_REQ_QUEUE_SIZE};
};

// TransportManager: 全局单例，负责所有 Transport 的创建和初始化
struct TransportManager {

  // AddTransport<Transport>(): 创建一个新的传输层实例并分配 ID
  template <typename Transport> static Transport *AddTransport() {
    return Instance().addTransport<Transport>();
  }

  // Init(): 启动所有已注册传输层的工作线程
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
  // 所有已创建的 Transport，以 unique_ptr 持有
  using TransportPool =
      etl::vector<etl::unique_ptr<TransportBase>, UROS_MAX_TRANSPORTS>;

  TransportPool tsps_;
};

} // namespace uros
