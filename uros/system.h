#pragma once

// ============================================================
// system.h — 系统节点 ID 管理
//
// uros 支持多节点分布式部署，每个运行 uros 的物理/逻辑节点
// 需要一个全局唯一的整型 ID（sys_id）。
//
// System 类以单例形式持有本节点的 ID，供消息路由在填写
// sys_src / sys_dst / sys_pre / sys_nxt 等字段时使用。
//
// 使用示例（初始化时设置一次）：
//   System::Id() = MY_NODE_ID;
// ============================================================

namespace uros {

struct System {
  // 返回本节点 ID 的可修改引用；初始化阶段赋值，运行时只读
  static int &Id() { return Instance().id(); }

protected:
  static System &Instance() {
    static System inst;
    return inst;
  }

  int &id() { return id_; }

protected:
  System() = default;
  System(const System &) = delete;
  System &operator=(const System &) = delete;

protected:
  int id_ = -1; // 未初始化状态为 -1
};

} // namespace uros
