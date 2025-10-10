#pragma once

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <mutex>
#include <stdio.h>
#include <thread>

#define UROS_ASSERT(expr) assert(expr)

#ifdef UROS_VERBOSE
#define UROS_PRINT printf
#else
#define UROS_PRINT (void)
#endif

namespace uros {

// EventBits 定义
using EventBits = uint32_t;

// Linux平台的EventGroup实现，模拟FreeRTOS EventGroup行为
struct EventGroup {
  EventGroup() : event_bits_(0) {}

  // 等待指定的事件位
  // bits_to_wait: 要等待的事件位掩码
  // clear_on_exit: 退出时是否清除匹配的事件位
  // wait_for_all: true=等待所有位都置位, false=等待任意位置位
  // timeout_ms: 超时时间（毫秒），-1表示无限等待
  // 返回值: 返回当前的事件位状态（无论是否超时）
  //        调用者需要检查返回值是否满足条件来判断是成功还是超时
  EventBits wait(const EventBits &bits_to_wait, bool clear_on_exit,
                 bool wait_for_all, int32_t timeout_ms = -1) {
    std::unique_lock<std::mutex> lock(mutex_);

    // 按值捕获，避免引用问题
    auto check_condition = [this, bits_to_wait, wait_for_all]() -> bool {
      if (wait_for_all) {
        // 等待所有指定的位都置位
        return (event_bits_ & bits_to_wait) == bits_to_wait;
      } else {
        // 等待任意指定的位置位
        return (event_bits_ & bits_to_wait) != 0;
      }
    };

    bool condition_met = false;

    if (timeout_ms < 0) {
      // 无限等待
      cv_.wait(lock, check_condition);
      condition_met = check_condition();
    } else if (timeout_ms == 0) {
      // 不等待，只检查当前状态
      condition_met = check_condition();
    } else {
      // 有超时等待
      UROS_PRINT("wait for %dms\n", timeout_ms);
      auto timeout = std::chrono::milliseconds(timeout_ms);
      condition_met = cv_.wait_for(lock, timeout, check_condition);
    }

    EventBits current_bits = event_bits_;

    // 只有在条件满足时才清除位（模拟FreeRTOS行为）
    if (condition_met && clear_on_exit) {
      event_bits_ &= ~bits_to_wait;
    }

    // 始终返回当前事件位状态（即使超时），这符合FreeRTOS行为
    return current_bits;
  }

  // 设置事件位
  // bits_to_set: 要设置的事件位掩码
  // 返回值: 设置前的事件位状态
  EventBits set(const EventBits &bits_to_set) {
    std::lock_guard<std::mutex> lock(mutex_);

    EventBits previous_bits = event_bits_;
    event_bits_ |= bits_to_set;

    // 通知所有等待的线程
    cv_.notify_all();

    return previous_bits;
  }

protected:
  mutable std::mutex mutex_;   // 保护事件位的互斥锁
  std::condition_variable cv_; // 条件变量，用于线程同步
  EventBits event_bits_;       // 当前事件位状态
};

struct CriticalLock {
  static void Lock() { GLock().lock(); }
  static void Unlock() { GLock().unlock(); }

protected:
  static std::mutex &GLock() {
    static std::mutex g_lock;
    return g_lock;
  }
};

template <typename TLock> struct LockGuard {
  LockGuard(TLock &lock, int32_t timeout_ms = -1) : lock_(lock) {
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

struct Thread {
  Thread(const char *name, const int32_t stack_size, const int32_t priority,
         const std::function<void(void)> &func) {
    // Linux 平台下简单封装 std::thread
    // 忽略 name, stack_size, priority 参数
    (void)name;
    (void)stack_size;
    (void)priority;

    thread_ = std::thread(func);
  }

  ~Thread() {
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  // 禁止拷贝和赋值
  Thread(const Thread &) = delete;
  Thread &operator=(const Thread &) = delete;

protected:
  std::thread thread_;
};

template <typename T, int N> struct Queue {
  bool push(const T &v, const int64_t timeout_ms = -1) {
    bool succeed = false;
    if (timeout_ms < 0) { // wait forever
      std::unique_lock<std::mutex> l(m_);
      while (_full()) {
        cv_.wait(l);
      }
      buffer_[(wr_ptr_++).idx] = v;
      succeed = true;
    } else if (timeout_ms == 0) { // no wait
      std::unique_lock<std::mutex> l(m_);
      if (!_full()) {
        buffer_[(wr_ptr_++).idx] = v;
        succeed = true;
      }
    } else { // wait for timeout
      std::unique_lock<std::mutex> l(m_);
      if (_full()) {
        cv_.wait_for(l, std::chrono::milliseconds(timeout_ms));
      }
      if (!_full()) { // check for spurious wakeup
        buffer_[(wr_ptr_++).idx] = v;
        succeed = true;
      }
    }

    if (succeed) {
      cv_.notify_all();
    }

    return succeed;
  }

  bool pop(T &v, const int64_t timeout_ms = -1) {
    bool succeed = false;
    if (timeout_ms < 0) { // wait forever
      std::unique_lock<std::mutex> l(m_);
      while (_empty()) {
        cv_.wait(l);
      }
      v = buffer_[(rd_ptr_++).idx];
      succeed = true;
    } else if (timeout_ms == 0) { // no wait
      std::unique_lock<std::mutex> l(m_);
      if (!_empty()) {
        v = buffer_[(rd_ptr_++).idx];
        succeed = true;
      }
    } else { // wait for timeout
      std::unique_lock<std::mutex> l(m_);
      if (_empty()) {
        cv_.wait_for(l, std::chrono::milliseconds(timeout_ms));
      }
      if (!_empty()) { // check for spurious wakeup
        v = buffer_[(rd_ptr_++).idx];
        succeed = true;
      }
    }

    if (succeed) {
      cv_.notify_all();
    }

    return succeed;
  }

  bool empty() const {
    std::unique_lock<std::mutex> l(m_);
    return _empty();
  }

  bool full() const {
    std::unique_lock<std::mutex> l(m_);
    return _full();
  }

  int size() const {
    std::unique_lock<std::mutex> l(m_);
    auto s = wr_ptr_.idx - rd_ptr_.idx;
    if (wr_ptr_.turn != rd_ptr_.turn) {
      s += N;
    }
    return s;
  }

  static int capacity() { return N; }

protected:
  bool _empty() const {
    return rd_ptr_.turn == wr_ptr_.turn && rd_ptr_.idx == wr_ptr_.idx;
  }

  bool _full() const {
    return rd_ptr_.turn != wr_ptr_.turn && rd_ptr_.idx == wr_ptr_.idx;
  }

protected:
  struct Ptr {
    int turn = 0;
    int idx = 0;

    // prefix ++
    Ptr &operator++() {
      ++idx;
      if (idx >= N) {
        idx = 0;
        ++turn;
      }
      return *this;
    }

    // postfix ++
    Ptr operator++(int) {
      auto old = *this;
      ++(*this);
      return old;
    }
  };

  Ptr rd_ptr_;
  Ptr wr_ptr_;
  std::array<T, N> buffer_;
  mutable std::mutex m_;
  mutable std::condition_variable cv_;
};

struct MessageBuffer {
  MessageBuffer(size_t capacity)
      : capacity_(capacity), read_pos_(0), read_turn_(0), write_pos_(0),
        write_turn_(0) {
    buffer_ = new uint8_t[capacity];
  }

  ~MessageBuffer() { delete[] buffer_; }

  // 发送消息到缓冲区
  // data: 消息数据指针
  // len: 消息长度
  // timeout_ms: 超时时间（毫秒），-1表示无限等待
  // 返回值: 实际发送的字节数（包含长度字段），0表示失败
  size_t send(const void *data, size_t len, int32_t timeout_ms = -1) {
    if (len == 0 || data == nullptr) {
      return 0;
    }

    // 消息格式: [length(4 bytes)][data(len bytes)]
    size_t total_size = sizeof(uint32_t) + len;

    std::unique_lock<std::mutex> lock(mutex_);

    auto has_space = [this, total_size]() -> bool {
      return get_free_space() >= total_size;
    };

    bool space_available = false;

    if (timeout_ms < 0) {
      // 无限等待
      cv_space_.wait(lock, has_space);
      space_available = true;
    } else if (timeout_ms == 0) {
      // 不等待
      space_available = has_space();
    } else {
      // 超时等待
      space_available = cv_space_.wait_for(
          lock, std::chrono::milliseconds(timeout_ms), has_space);
    }

    if (!space_available) {
      return 0; // 没有足够空间
    }

    // 写入消息长度（4字节）
    uint32_t msg_len = static_cast<uint32_t>(len);
    write_bytes(reinterpret_cast<const uint8_t *>(&msg_len), sizeof(msg_len));

    // 写入消息数据
    write_bytes(static_cast<const uint8_t *>(data), len);

    // 通知接收者
    cv_data_.notify_one();

    return total_size;
  }

  // 从缓冲区接收消息
  // data: 接收缓冲区指针
  // len: 接收缓冲区大小
  // timeout_ms: 超时时间（毫秒），-1表示无限等待
  // 返回值: 实际接收的消息长度（不包含长度字段），0表示失败或超时
  size_t recv(void *data, size_t len, int32_t timeout_ms = -1) {
    if (data == nullptr || len == 0) {
      return 0;
    }

    std::unique_lock<std::mutex> lock(mutex_);

    auto has_data = [this]() -> bool {
      return get_available_data() >= sizeof(uint32_t);
    };

    bool data_available = false;

    if (timeout_ms < 0) {
      // 无限等待
      cv_data_.wait(lock, has_data);
      data_available = true;
    } else if (timeout_ms == 0) {
      // 不等待
      data_available = has_data();
    } else {
      // 超时等待
      data_available = cv_data_.wait_for(
          lock, std::chrono::milliseconds(timeout_ms), has_data);
    }

    if (!data_available) {
      return 0; // 没有数据
    }

    // 读取消息长度
    uint32_t msg_len = 0;
    read_bytes(reinterpret_cast<uint8_t *>(&msg_len), sizeof(msg_len));

    // 读取消息数据
    read_bytes(static_cast<uint8_t *>(data), msg_len);

    // 通知发送者有空间了
    cv_space_.notify_one();

    return msg_len;
  }

  // 获取可用空间（字节）
  size_t get_free_space() const {
    size_t used = get_available_data();
    return capacity_ - used;
  }

  // 获取可用数据（字节）
  size_t get_available_data() const {
    if (write_turn_ == read_turn_) {
      return write_pos_ - read_pos_;
    } else if (write_turn_ > read_turn_) {
      return capacity_ - read_pos_ + write_pos_;
    } else {
      // 这种情况理论上不可能发生，表示读写指针状态异常
      return 0;
    }
  }

  // 检查是否为空
  bool empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return read_pos_ == write_pos_ && read_turn_ == write_turn_;
  }

  // 获取容量
  size_t capacity() const { return capacity_; }

  // 禁止拷贝
  MessageBuffer(const MessageBuffer &) = delete;
  MessageBuffer &operator=(const MessageBuffer &) = delete;

protected:
  // 写入字节到环形缓冲区
  void write_bytes(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
      buffer_[write_pos_++] = data[i];
      if (write_pos_ >= capacity_) {
        write_pos_ = 0;
        write_turn_++;
      }
    }
  }

  // 从环形缓冲区读取字节
  void read_bytes(uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
      data[i] = buffer_[read_pos_++];
      if (read_pos_ >= capacity_) {
        read_pos_ = 0;
        read_turn_++;
      }
    }
  }

  uint8_t *buffer_;                  // 环形缓冲区
  size_t capacity_;                  // 缓冲区容量
  size_t read_pos_;                  // 读位置
  size_t read_turn_;                 // 读位置圈数
  size_t write_pos_;                 // 写位置
  size_t write_turn_;                // 写位置圈数
  mutable std::mutex mutex_;         // 互斥锁
  std::condition_variable cv_data_;  // 有数据的条件变量
  std::condition_variable cv_space_; // 有空间的条件变量
};
} // namespace uros
