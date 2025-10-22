#pragma once

#include <cstdint>

namespace uros {

template <typename TReq, typename TRsp> struct ServiceT;

struct ClientBase {
  ClientBase(int id) : id_(id) {}

  auto &id() const { return id_; }

  virtual ~ClientBase() = default;

protected:
  int id_;
};

template <typename TReq, typename TRsp> struct ClientT : ClientBase {
  using Service = ServiceT<TReq, TRsp>;

  ClientT(int id) : ClientBase(id) {}

  void connect(Service *service);

  bool call(const TReq &req, TRsp &rsp, int timeout_ms);

protected:
  Service *service_;
  int seq_ = 0;
};

} // namespace uros
