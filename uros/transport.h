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

struct TransportInterface {};

template <typename Derived> struct TransportBase : public TransportInterface {

  bool declareTopic(const char *topic_name);

  void init() { derived().initImpl(); }

  // write to transport should be non-blocking
  template <typename Topic>
  void write(Topic *topic, const typename Topic::Msg &msg) {
    derived().writeImpl(topic, msg);
  }

  int32_t &id() { return id_; }
  const int32_t &id() const { return id_; }

protected:
  Derived &derived() { return static_cast<Derived &>(*this); }
  const Derived &derived() const { return static_cast<const Derived &>(*this); }

protected:
  int32_t id_ = -1; // unique id of transport, used for msg routing
  etl::array<TopicMeta, UROS_MAX_TOPICS> topic_metas_;
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

  static auto &Transports() { return Instance().transports(); }

protected:
  template <size_t Idx> auto &transport() { return std::get<Idx>(transports_); }

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
