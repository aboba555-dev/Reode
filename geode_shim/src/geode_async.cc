// Copyright 2026 The Reode Authors
//
// geode::async::runtime(). Every Geode mod's entry point registers this
// runtime as its global arc runtime, but only mods that actually spawn async
// tasks use it.
//
// TODO(reode): back this with a real arc::Runtime. Until then a mod that
// spawns a task will crash; the warning below makes that visible in the log.

#include <cstddef>
#include <new>

#include "Geode/utils/async.hpp"
#include "host.h"

namespace geode::async {

arc::Runtime& runtime() {
  alignas(std::max_align_t) static std::byte storage[sizeof(arc::Runtime)] = {};
  static bool warned = false;
  if (!warned) {
    warned = true;
    reode::geode_shim::Log(nullptr, kReodeLogDebug,
                           "async runtime requested; Reode provides a "
                           "placeholder, async tasks are not supported yet");
  }
  return *std::launder(reinterpret_cast<arc::Runtime*>(storage));
}

}  // namespace geode::async
