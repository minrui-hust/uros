#pragma once

#include "publisher.h"
#include "topic.h"
#include "transport_local.h"

namespace uros {

template <typename Msg> void PublisherT<Msg>::advertise(TopicT<Msg> *topic) {
  topic_ = topic;
}

template <typename TMsg> void PublisherT<TMsg>::publish(const TMsg &msg) {
  msg.__meta__.participant = id_;
  msg.__meta__.type = MsgType::MsgTypeNormal;
  TransportManager::GetTransportLocal()->write(topic_, msg);
}

} // namespace uros
