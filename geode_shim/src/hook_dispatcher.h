// Copyright 2026 The Reode Authors

#ifndef REODE_GEODE_SHIM_SRC_HOOK_DISPATCHER_H_
#define REODE_GEODE_SHIM_SRC_HOOK_DISPATCHER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "reode/api.h"

namespace reode::geode_shim {

// How a hooked function receives its arguments, derived from the Geode hook
// metadata of the first hook placed on it.
struct FunctionShape {
  // Arguments (including `this` and a hidden return pointer) that do not
  // fit into the four argument registers.
  std::size_t stack_argument_count = 0;
  // Member function returning a class by value: the target has `this` in
  // rcx and the return pointer in rdx, while Geode detours are plain
  // functions expecting the opposite order.
  bool swap_this_and_return = false;
};

class HookTarget;

// Returns a stub that swaps rcx and rdx and jumps to `target`: lets a plain
// function call a member function that returns a class by value.
void* MakeSwapThunk(std::uintptr_t target);

// Runs Geode detours the way Geode mods expect.
//
// A Geode detour calls the original by calling the hooked address again.
// Every hooked address therefore gets a generated handler that keeps a
// per-thread depth counter: the first entry runs detour 0, a call from
// inside detour 0 runs detour 1, and so on, until the chain is exhausted and
// the real function runs. The handler is installed as an ordinary Reode
// hook, so Geode and Reode mods can hook the same function.
class HookDispatcher {
 public:
  static HookDispatcher& Instance();

  HookDispatcher(const HookDispatcher&) = delete;
  HookDispatcher& operator=(const HookDispatcher&) = delete;

  // Adds `detour` to the chain of `address`. Lower priorities run first,
  // as in Geode. `owner` is used for the underlying Reode hook.
  ReodeStatus Add(void* address, void* detour, std::int32_t priority,
                  const FunctionShape& shape, ReodeMod* owner);
  ReodeStatus Remove(void* address, void* detour);

 private:
  HookDispatcher() = default;
  ~HookDispatcher() = default;

  HookTarget* FindOrCreate(void* address, const FunctionShape& shape,
                           ReodeMod* owner, ReodeStatus* status);

  std::mutex mutex_;
  std::vector<std::unique_ptr<HookTarget>> targets_;
};

}  // namespace reode::geode_shim

#endif  // REODE_GEODE_SHIM_SRC_HOOK_DISPATCHER_H_
