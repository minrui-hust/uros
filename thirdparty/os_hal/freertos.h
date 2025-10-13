#pragma once

#include <cstdint>
#include <functional>

#include "FreeRTOS.h"

#include "atomic.h"
#include "event_groups.h"
#include "list.h"
#include "message_buffer.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"

#define CHECK(expr) configASSERT(expr)

using type_id_t = uint32_t;

struct CriticalLock {
  static void Lock() { vPortEnterCritical(); }
  static void Unlock() { vPortExitCritical(); }
};

struct Mutex {
  Mutex() {
    sem_ = xSemaphoreCreateMutex();
    configASSERT(sem_);
  }

  bool lock(int timeout_ms = -1) {
    TickType_t ticks_to_wait =
        timeout_ms < 0 ? portMAX_DELAY : timeout_ms / portTICK_PERIOD_MS;
    return xSemaphoreTake(sem_, ticks_to_wait);
  }

  void unlock() { xSemaphoreGive(sem_); }

protected:
  SemaphoreHandle_t sem_;
};

struct BinarySemaphore {
  BinarySemaphore() {
    sem_ = xSemaphoreCreateBinary();
    configASSERT(sem_);
  }

  void give() { xSemaphoreGive(sem_); }

  bool take(int timeout_ms = -1) {
    TickType_t ticks_to_wait =
        timeout_ms < 0 ? portMAX_DELAY : timeout_ms / portTICK_PERIOD_MS;
    return xSemaphoreTake(sem_, ticks_to_wait);
  }

protected:
  SemaphoreHandle_t sem_;
};

using EventBits = EventBits_t;

struct EventGroup {
  EventGroup() {
    evt_ = xEventGroupCreate();
    configASSERT(evt_);
  }

  EventBits_t wait(const EventBits_t &bits_to_wait, bool clear_on_exit,
                   bool wait_for_all, int timeout_ms = -1) {
    TickType_t ticks_to_wait =
        timeout_ms < 0 ? portMAX_DELAY : timeout_ms / portTICK_PERIOD_MS;
    return xEventGroupWaitBits(evt_, bits_to_wait, clear_on_exit, wait_for_all,
                               ticks_to_wait);
  }

  EventBits_t set(const EventBits_t &bits_to_set) {
    return xEventGroupSetBits(evt_, bits_to_set);
  }

protected:
  EventGroupHandle_t evt_;
};

inline void ThreadTask(void *param) {
  auto &func = *reinterpret_cast<std::function<void(void)> *>(param);

  func();

  while (true) {
    vTaskSuspend(NULL);
  }
}

struct Thread {
  Thread(const char *name, const int stack_size, const int priority,
         const std::function<void(void)> &func) {
    func_ = func;
    auto res =
        xTaskCreate(ThreadTask, name, stack_size, &func_, priority, &task_);
    configASSERT(res);
  }

  ~Thread() { vTaskDelete(task_); }

protected:
  TaskHandle_t task_;
  std::function<void(void)> func_;
};

inline void msleep(int timeout_ms) {
  TickType_t ticks_to_wait =
      timeout_ms < 0 ? portMAX_DELAY : timeout_ms / portTICK_PERIOD_MS;
  vTaskDelay(ticks_to_wait);
}
