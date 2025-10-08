#pragma once

namespace uros {

template <typename TransportManager, typename TMsg> struct TopicT;

struct PublisherBase {};

template <typename TransportManager, typename TMsg>
struct PublisherT : public PublisherBase {
  using Topic = TopicT<TransportManager, TMsg>;

  void advertise(Topic *topic);

  void publish(const TMsg &msg);

protected:
  Topic *topic_;
};

} // namespace uros
