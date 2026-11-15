// Copyright 2026 The Reode Authors

#ifndef REODE_LOADER_SRC_HOOK_NEAR_ALLOCATOR_H_
#define REODE_LOADER_SRC_HOOK_NEAR_ALLOCATOR_H_

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace reode::hook {

// Hands out small blocks of executable memory located within +-2 GiB of a
// given address, so that 5-byte rel32 jumps and rip-relative operands can
// reach them. Memory is never returned to the system: trampolines must stay
// valid for as long as any thread might still be executing them.
class NearAllocator {
 public:
  NearAllocator() = default;
  NearAllocator(const NearAllocator&) = delete;
  NearAllocator& operator=(const NearAllocator&) = delete;

  // Returns `size` bytes of RWX memory aligned to 16 bytes, or nullptr.
  void* Allocate(std::uintptr_t near_address, std::size_t size);

 private:
  struct Region {
    std::uintptr_t base;
    std::size_t used;
  };

  static bool IsReachable(std::uintptr_t from, std::uintptr_t to,
                          std::size_t size);
  static std::uintptr_t ReserveNear(std::uintptr_t near_address);

  std::mutex mutex_;
  std::vector<Region> regions_;
};

}  // namespace reode::hook

#endif  // REODE_LOADER_SRC_HOOK_NEAR_ALLOCATOR_H_
