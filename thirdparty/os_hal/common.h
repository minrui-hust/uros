#pragma once

template <typename TLock> struct LockGuard {
  LockGuard(TLock &lock, int timeout_ms = -1) : lock_(lock) {
    locked_ = lock_.lock(timeout_ms);
  }

  bool locked() const { return locked_; }

  ~LockGuard() {
    if (locked_) {
      lock_.unlock();
    }
  }

protected:
  TLock &lock_;
  bool locked_ = false;
};

template <> struct LockGuard<CriticalLock> {
  LockGuard() { CriticalLock::Lock(); }
  ~LockGuard() { CriticalLock::Unlock(); }
};
