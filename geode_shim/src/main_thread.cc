// Copyright 2026 The Reode Authors

#include "main_thread.h"

#include <windows.h>

#include <functional>
#include <mutex>
#include <utility>
#include <vector>

#include "host.h"

namespace reode::geode_shim {
namespace {

using SchedulerUpdate = void (*)(void* self, float delta);

constexpr char kSchedulerUpdate[] = "?update@CCScheduler@cocos2d@@UEAAXM@Z";

constinit SchedulerUpdate g_original_update = nullptr;

struct Queue {
  std::mutex mutex;
  std::vector<std::move_only_function<void()>> functions;
};

Queue& GetQueue() {
  static Queue* const kQueue = new Queue();
  return *kQueue;
}

void UpdateDetour(void* self, float delta) {
  std::vector<std::move_only_function<void()>> pending;
  {
    Queue& queue = GetQueue();
    std::lock_guard lock(queue.mutex);
    pending.swap(queue.functions);
  }
  // Functions queued while these run wait for the next frame.
  for (auto& function : pending) function();
  g_original_update(self, delta);
}

}  // namespace

void InstallMainThreadQueue() {
  HMODULE cocos = GetModuleHandleW(L"libcocos2d.dll");
  void* target =
      cocos != nullptr
          ? reinterpret_cast<void*>(GetProcAddress(cocos, kSchedulerUpdate))
          : nullptr;
  ReodeStatus status =
      target == nullptr
          ? kReodeNotFound
          : Api().create_hook(
                nullptr, target, reinterpret_cast<void*>(&UpdateDetour), 0,
                reinterpret_cast<void**>(&g_original_update), nullptr);
  if (status != kReodeOk) {
    Log(nullptr, kReodeLogError,
        "could not hook CCScheduler::update; queueInMainThread will not run");
  }
}

void QueueInMainThread(std::move_only_function<void()> function) {
  Queue& queue = GetQueue();
  std::lock_guard lock(queue.mutex);
  queue.functions.push_back(std::move(function));
}

}  // namespace reode::geode_shim
