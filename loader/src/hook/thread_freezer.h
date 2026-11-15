// Copyright 2026 The Reode Authors

#ifndef REODE_LOADER_SRC_HOOK_THREAD_FREEZER_H_
#define REODE_LOADER_SRC_HOOK_THREAD_FREEZER_H_

#include <windows.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace reode::hook {

// Suspends every other thread of the process for the lifetime of the object,
// so code can be patched without another thread executing half-written
// instructions.
class ThreadFreezer {
 public:
  ThreadFreezer();
  ~ThreadFreezer();

  ThreadFreezer(const ThreadFreezer&) = delete;
  ThreadFreezer& operator=(const ThreadFreezer&) = delete;

  // Calls `relocate` with the instruction pointer of every frozen thread and
  // writes back the returned value. Used to move threads that are parked
  // inside bytes that are about to be overwritten.
  void RelocateInstructionPointers(
      const std::function<std::uintptr_t(std::uintptr_t)>& relocate);

 private:
  std::vector<HANDLE> threads_;
};

}  // namespace reode::hook

#endif  // REODE_LOADER_SRC_HOOK_THREAD_FREEZER_H_
