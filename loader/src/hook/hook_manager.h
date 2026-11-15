// Copyright 2026 The Reode Authors

#ifndef REODE_LOADER_SRC_HOOK_HOOK_MANAGER_H_
#define REODE_LOADER_SRC_HOOK_HOOK_MANAGER_H_

#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "hook/near_allocator.h"
#include "reode/api.h"

namespace reode::hook {

// Read-only snapshot of one hook, for diagnostics and the in-game UI.
struct HookInfo {
  std::uintptr_t target = 0;
  std::uintptr_t detour = 0;
  std::int32_t priority = 0;
  bool enabled = false;
  std::string owner;
};

// Inline function hooking with support for any number of hooks per target.
//
// The first hook on a target overwrites the start of the function with a
// 5-byte `jmp rel32` to an "entry stub" and relocates the overwritten
// instructions into a trampoline. Every hook owns a "next stub" that its
// detour calls as the original function. All stubs are `jmp [rip+2]` with an
// 8-byte aligned pointer that is updated atomically, so hooks can be added,
// enabled, disabled and removed in any order while the game runs:
//
//   target -> entry stub -> detour A -> A.next -> detour B -> B.next
//          -> trampoline (relocated prologue) -> rest of target
class HookManager {
 public:
  class Hook;

  static HookManager& Instance();

  HookManager(const HookManager&) = delete;
  HookManager& operator=(const HookManager&) = delete;

  // Hooks `target`. On success `*original` points to the next function in
  // the chain; it is written before the hook becomes reachable.
  std::expected<Hook*, ReodeStatus> Create(void* target, void* detour,
                                           std::int32_t priority,
                                           std::string owner, void** original);
  ReodeStatus SetEnabled(Hook* hook, bool enabled);
  ReodeStatus Remove(Hook* hook);

  std::vector<HookInfo> List() const;

 private:
  struct Target;
  struct JumpStub;

  HookManager() = default;
  ~HookManager() = default;

  std::expected<Target*, ReodeStatus> GetOrCreateTarget(std::uintptr_t address);
  JumpStub* AllocateStub(std::uintptr_t near_address,
                         std::uintptr_t destination);
  static void RelinkChain(Target& target);
  bool OwnsHook(const Hook* hook) const;

  mutable std::mutex mutex_;
  NearAllocator allocator_;
  std::vector<std::unique_ptr<Target>> targets_;
};

}  // namespace reode::hook

#endif  // REODE_LOADER_SRC_HOOK_HOOK_MANAGER_H_
