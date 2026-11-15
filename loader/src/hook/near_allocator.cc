// Copyright 2026 The Reode Authors

#include "hook/near_allocator.h"

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <mutex>

namespace reode::hook {
namespace {

constexpr std::size_t kRegionSize = 64 * 1024;
// Keep a safety margin below 2 GiB so any offset inside a region fits rel32.
constexpr std::uintptr_t kMaxDistance = 0x7FF00000;

std::uintptr_t AlignUp(std::uintptr_t value, std::uintptr_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

std::uintptr_t AlignDown(std::uintptr_t value, std::uintptr_t alignment) {
  return value & ~(alignment - 1);
}

}  // namespace

bool NearAllocator::IsReachable(std::uintptr_t from, std::uintptr_t to,
                                std::size_t size) {
  std::uintptr_t low = from < to ? from : to;
  std::uintptr_t high = from < to ? to + size : from;
  return high - low < kMaxDistance;
}

std::uintptr_t NearAllocator::ReserveNear(std::uintptr_t near_address) {
  SYSTEM_INFO info{};
  GetSystemInfo(&info);
  const std::uintptr_t granularity = info.dwAllocationGranularity;
  const auto min_app =
      reinterpret_cast<std::uintptr_t>(info.lpMinimumApplicationAddress);
  const auto max_app =
      reinterpret_cast<std::uintptr_t>(info.lpMaximumApplicationAddress);

  std::uintptr_t low =
      near_address > kMaxDistance ? near_address - kMaxDistance : min_app;
  std::uintptr_t high = near_address + kMaxDistance;
  if (low < min_app) low = min_app;
  if (high > max_app) high = max_app;

  // Walk outwards from the target, alternating directions, and try every
  // free region we encounter.
  std::uintptr_t down = AlignDown(near_address, granularity);
  std::uintptr_t up = AlignUp(near_address, granularity);
  while (down > low || up < high) {
    for (std::uintptr_t* cursor : {&down, &up}) {
      bool going_down = cursor == &down;
      if (going_down ? *cursor <= low : *cursor >= high) continue;
      MEMORY_BASIC_INFORMATION mbi{};
      if (VirtualQuery(reinterpret_cast<void*>(*cursor), &mbi, sizeof(mbi)) ==
          0) {
        return 0;
      }
      auto region_base = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
      if (mbi.State == MEM_FREE) {
        std::uintptr_t candidate = AlignUp(*cursor, granularity);
        if (candidate + kRegionSize <= region_base + mbi.RegionSize &&
            IsReachable(near_address, candidate, kRegionSize)) {
          void* memory =
              VirtualAlloc(reinterpret_cast<void*>(candidate), kRegionSize,
                           MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
          if (memory != nullptr) {
            return reinterpret_cast<std::uintptr_t>(memory);
          }
        }
      }
      if (going_down) {
        if (region_base < granularity) {
          *cursor = low;
        } else {
          *cursor = AlignDown(region_base - 1, granularity);
        }
      } else {
        *cursor = AlignUp(region_base + mbi.RegionSize, granularity);
      }
    }
  }
  return 0;
}

void* NearAllocator::Allocate(std::uintptr_t near_address, std::size_t size) {
  size = AlignUp(size, 16);
  if (size > kRegionSize) return nullptr;
  std::lock_guard lock(mutex_);
  for (Region& region : regions_) {
    if (region.used + size <= kRegionSize &&
        IsReachable(near_address, region.base, kRegionSize)) {
      void* result = reinterpret_cast<void*>(region.base + region.used);
      region.used += size;
      return result;
    }
  }
  std::uintptr_t base = ReserveNear(near_address);
  if (base == 0) return nullptr;
  regions_.push_back({base, size});
  return reinterpret_cast<void*>(base);
}

}  // namespace reode::hook
