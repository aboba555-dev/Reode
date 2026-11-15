// Copyright 2026 The Reode Authors

#ifndef REODE_GEODE_SHIM_SRC_MAIN_THREAD_H_
#define REODE_GEODE_SHIM_SRC_MAIN_THREAD_H_

#include <functional>

namespace reode::geode_shim {

// Hooks cocos2d::CCScheduler::update so queued functions run on the main
// thread once per frame. Call once during initialization.
void InstallMainThreadQueue();

// Queues `function` to run on the main thread during the next frame.
void QueueInMainThread(std::move_only_function<void()> function);

}  // namespace reode::geode_shim

#endif  // REODE_GEODE_SHIM_SRC_MAIN_THREAD_H_
