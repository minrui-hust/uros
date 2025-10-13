#pragma once

namespace uros {

template <typename TMsg> struct TopicT;

struct PublisherBase {};

template <typename TMsg> struct PublisherT : public PublisherBase {
  using Topic = TopicT<TMsg>;
  using Msg = TMsg;

  void advertise(Topic *topic);

  void publish(const Msg &msg);

protected:
  Topic *topic_;
};

} // namespace uros
