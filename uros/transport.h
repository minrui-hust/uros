#pragma once

#include <cstddef>
#include <cstdint>
#include <tuple>

#include "etl/vector.h"

#include "platform.h"

namespace uros {

struct TopicBase;

struct TopicMeta {
  const char *name = "";
  TopicBase *topic = nullptr;
};

struct ServiceMeta {};

struct TransportInterface {};

template <typename Derived> struct TransportBase : public TransportInterface {

  bool declareTopic(const char *topic_name);

  int32_t &id() { return id_; }
  const int32_t &id() const { return id_; }

  void init() { derived().initImpl(); }

  // write to transport should be non-blocking
  template <typename Topic>
  void write(Topic *topic, const typename Topic::Msg &msg) {
    derived().writeImpl(topic, msg);
  }

  // send service request to transport
  template <typename Service>
  bool sendRequest(Service *service, const typename Service::Req &req,
                   int timeout_ms) {
    derived().sendRequestImpl(service, req, timeout_ms);
  }

protected:
  Derived &derived() { return static_cast<Derived &>(*this); }
  const Derived &derived() const { return static_cast<const Derived &>(*this); }

  void initImpl() {} // default version doing nothing

protected:
  int32_t id_ = -1; // unique id of transport, used for msg routing
  etl::array<TopicBase *, UROS_MAX_TOPICS> topics_;
  etl::array<ServiceMeta, UROS_MAX_SERVICES> service_metas_;
};

template <typename... TTransports> struct TransportManagerT {
  // Get the type of the Idx-th transport
  template <size_t Idx>
  using TransportType = std::tuple_element_t<Idx, std::tuple<TTransports...>>;

  template <template <typename> class Transform,
            template <typename...> class Target>
  using ApplyTransports = Target<typename Transform<TTransports>::type...>;

  static constexpr size_t size = sizeof...(TTransports);

  static TransportManagerT &Instance() {
    static TransportManagerT inst;
    return inst;
  }

  static void Init() { return Instance().init(); }

  template <size_t Idx> static auto &Transport() {
    return Instance().template transport<Idx>();
  }

  template <typename TTransport> static auto &Transport() {
    return Instance().template transport<TTransport>();
  }

  static auto &Transports() { return Instance().transports(); }

protected:
  template <size_t Idx> auto &transport() { return std::get<Idx>(transports_); }

  template <typename TTransport> auto &transport() {
    return std::get<TTransport>(transports_);
  }

  auto &transports() { return transports_; }

  void init();

protected:
  TransportManagerT();
  TransportManagerT(const TransportManagerT &other) = delete;
  TransportManagerT &operator=(const TransportManagerT &other) = delete;

protected:
  std::tuple<TTransports...> transports_;
};

} // namespace uros
