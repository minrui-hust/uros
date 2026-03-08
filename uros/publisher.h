#pragma once

// ============================================================
// publisher.h — 发布者（Publisher）定义
//
// Publisher 是话题（Topic）写入端的句柄。
// 用户通过 Node::createPublisher<Msg>(topic_name) 获取，
// 之后调用 publish(msg) 将消息写入话题并触发通知。
//
// 设计要点：
//   - Publisher 由 Topic 持有（通过 addPublisher() 创建），
//     Node 只持有指向它的裸指针，生命周期由 Topic 管理。
//   - id_ 是该 Publisher 在所属 Topic 的 pubs_ 列表中的索引。
//   - 每次 publish() 会自动填写 MsgMeta（类型、源节点、entry_hash
//     等），然后调用 Topic::write() 完成本地投递和跨传输层转发。
// ============================================================

namespace uros {

template <typename TMsg> struct TopicT;

// PublisherBase: 所有具体发布者的基类，持有在 Topic 内的索引 id_
struct PublisherBase {
  PublisherBase(int id) : id_(id) {}

  virtual ~PublisherBase() = default;

protected:
  int id_; // 在 topic.pubs_ 数组中的下标
};

// PublisherT<TMsg>: 模板化发布者，绑定到特定消息类型的 TopicT<TMsg>
template <typename TMsg> struct PublisherT : public PublisherBase {
  using Topic = TopicT<TMsg>;
  using Msg = TMsg;

  PublisherT(int id) : PublisherBase(id) {}

  // advertise(): 将 Publisher 与对应 Topic 关联，由 Topic::addPublisher() 调用
  void advertise(Topic *topic);

  // publish(): 填写消息元数据并调用 Topic::write()
  // Topic::write() 会先更新本地缓存，再通知所有本地订阅者和远端传输层
  void publish(const Msg &msg);

protected:
  Topic *topic_; // 关联的 Topic 指针
};

} // namespace uros
