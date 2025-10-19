#pragma once

namespace uros {

struct System {
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
  int id_;
};

} // namespace uros
