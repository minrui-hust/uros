#pragma once

namespace uros {

template <typename TMsg> struct TopicT;

struct PublisherBase {
  PublisherBase(int id) : id_(id) {}

protected:
  int id_;
};

template <typename TMsg> struct PublisherT : public PublisherBase {
  using Topic = TopicT<TMsg>;
  using Msg = TMsg;

  PublisherT(int id) : PublisherBase(id) {}

  void advertise(Topic *topic);

  void publish(const Msg &msg);

protected:
  Topic *topic_;
};

} // namespace uros
