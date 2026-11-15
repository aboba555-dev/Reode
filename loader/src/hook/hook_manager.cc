// Copyright 2026 The Reode Authors

#include "hook/hook_manager.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <expected>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "hook/thread_freezer.h"
#include "hook/x64_decoder.h"

namespace reode::hook {
namespace {

constexpr std::size_t kRel32JumpSize = 5;
constexpr std::size_t kAbsoluteJumpSize = 14;  // jmp [rip+0]; dq target
constexpr std::size_t kTrampolineCapacity = 160;
constexpr std::size_t kMaxStolenBytes = 32;

struct InstructionMapping {
  std::size_t original_offset;
  std::size_t trampoline_offset;
};

class CodeWriter {
 public:
  CodeWriter(std::uint8_t* buffer, std::size_t capacity)
      : buffer_(buffer), capacity_(capacity) {}

  bool Bytes(const void* data, std::size_t size) {
    if (size_ + size > capacity_) return false;
    std::memcpy(buffer_ + size_, data, size);
    size_ += size;
    return true;
  }
  bool Byte(std::uint8_t value) { return Bytes(&value, 1); }
  bool Qword(std::uint64_t value) { return Bytes(&value, sizeof(value)); }

  // jmp qword ptr [rip+0]; dq destination
  bool AbsoluteJump(std::uint64_t destination) {
    static constexpr std::uint8_t kJump[] = {0xFF, 0x25, 0, 0, 0, 0};
    return Bytes(kJump, sizeof(kJump)) && Qword(destination);
  }

  std::size_t size() const { return size_; }
  std::uint8_t* current() const { return buffer_ + size_; }

 private:
  std::uint8_t* buffer_;
  std::size_t capacity_;
  std::size_t size_ = 0;
};

bool FitsInt32(std::int64_t value) {
  return value >= INT32_MIN && value <= INT32_MAX;
}

// Copies whole instructions from `target` into `trampoline` until at least
// kRel32JumpSize bytes are covered, rewriting everything position-dependent,
// then appends a jump back to the remainder of the original function.
std::expected<std::size_t, ReodeStatus> BuildTrampoline(
    std::uintptr_t target, std::uint8_t* trampoline,
    std::vector<InstructionMapping>* mappings) {
  std::array<std::uint8_t, kMaxStolenBytes + 16> code{};
  std::memcpy(code.data(), reinterpret_cast<const void*>(target), code.size());

  // First pass: find the instructions to steal.
  std::vector<DecodedInstruction> instructions;
  std::size_t stolen = 0;
  while (stolen < kRel32JumpSize) {
    auto decoded =
        DecodeInstruction(std::span(code).subspan(stolen), target + stolen);
    if (!decoded) return std::unexpected(kReodeUnsupportedInstruction);
    instructions.push_back(*decoded);
    stolen += decoded->length;
  }

  // Second pass: emit relocated copies.
  CodeWriter writer(trampoline, kTrampolineCapacity);
  std::size_t offset = 0;
  for (const DecodedInstruction& instruction : instructions) {
    mappings->push_back({offset, writer.size()});
    const std::uint8_t* bytes = code.data() + offset;
    const auto new_address = reinterpret_cast<std::uintptr_t>(writer.current());

    if (instruction.branch != BranchKind::kNone) {
      std::uint64_t destination = instruction.branch_target;
      if (destination > target && destination < target + stolen) {
        // Jumps into the bytes we are about to overwrite cannot be kept.
        return std::unexpected(kReodeUnsupportedInstruction);
      }
      bool ok = false;
      switch (instruction.branch) {
        case BranchKind::kJump:
          ok = writer.AbsoluteJump(destination);
          break;
        case BranchKind::kCall: {
          // call [rip+2]; jmp +8; dq destination
          static constexpr std::uint8_t kCall[] = {0xFF, 0x15, 0x02, 0x00,
                                                   0x00, 0x00, 0xEB, 0x08};
          ok = writer.Bytes(kCall, sizeof(kCall)) && writer.Qword(destination);
          break;
        }
        case BranchKind::kConditional:
          // j!cc +14; jmp [rip+0]; dq destination
          ok = writer.Byte(static_cast<std::uint8_t>(
                   0x70 | (instruction.condition ^ 1))) &&
               writer.Byte(static_cast<std::uint8_t>(kAbsoluteJumpSize)) &&
               writer.AbsoluteJump(destination);
          break;
        case BranchKind::kLoop:
        case BranchKind::kNone:
          return std::unexpected(kReodeUnsupportedInstruction);
      }
      if (!ok) return std::unexpected(kReodeOutOfMemory);
    } else if (instruction.rip_relative) {
      std::int64_t displacement = static_cast<std::int64_t>(
          instruction.rip_target - (new_address + instruction.length));
      if (!FitsInt32(displacement)) {
        return std::unexpected(kReodeUnsupportedInstruction);
      }
      std::uint8_t* out = writer.current();
      if (!writer.Bytes(bytes, instruction.length)) {
        return std::unexpected(kReodeOutOfMemory);
      }
      auto displacement32 = static_cast<std::int32_t>(displacement);
      std::memcpy(out + instruction.displacement_offset, &displacement32,
                  sizeof(displacement32));
    } else if (!writer.Bytes(bytes, instruction.length)) {
      return std::unexpected(kReodeOutOfMemory);
    }
    offset += instruction.length;
  }
  if (!writer.AbsoluteJump(target + stolen)) {
    return std::unexpected(kReodeOutOfMemory);
  }
  return stolen;
}

bool WriteCode(std::uintptr_t address, const void* bytes, std::size_t size) {
  auto* destination = reinterpret_cast<void*>(address);
  DWORD old_protect = 0;
  if (!VirtualProtect(destination, size, PAGE_EXECUTE_READWRITE,
                      &old_protect)) {
    return false;
  }
  std::memcpy(destination, bytes, size);
  VirtualProtect(destination, size, old_protect, &old_protect);
  FlushInstructionCache(GetCurrentProcess(), destination, size);
  return true;
}

}  // namespace

// jmp qword ptr [rip+2]; int3; int3; dq destination
struct alignas(16) HookManager::JumpStub {
  std::uint8_t code[8];
  volatile std::int64_t destination;

  void Set(std::uintptr_t value) {
    InterlockedExchange64(&destination, static_cast<std::int64_t>(value));
  }
};
static_assert(sizeof(std::uintptr_t) == sizeof(std::int64_t));

class HookManager::Hook {
 public:
  Target* target = nullptr;
  std::uintptr_t detour = 0;
  std::int32_t priority = 0;
  std::uint64_t sequence = 0;
  bool enabled = true;
  std::string owner;
  JumpStub* next_stub = nullptr;
};

struct HookManager::Target {
  std::uintptr_t address = 0;
  std::uint8_t* trampoline = nullptr;
  JumpStub* entry_stub = nullptr;
  // Sorted by descending priority, then by creation order.
  std::vector<std::unique_ptr<Hook>> hooks;
};

HookManager& HookManager::Instance() {
  static HookManager* const kInstance = new HookManager();
  return *kInstance;
}

HookManager::JumpStub* HookManager::AllocateStub(std::uintptr_t near_address,
                                                 std::uintptr_t destination) {
  auto* stub = static_cast<JumpStub*>(
      allocator_.Allocate(near_address, sizeof(JumpStub)));
  if (stub == nullptr) return nullptr;
  static constexpr std::uint8_t kCode[8] = {0xFF, 0x25, 0x02, 0x00,
                                            0x00, 0x00, 0xCC, 0xCC};
  std::memcpy(stub->code, kCode, sizeof(kCode));
  stub->Set(destination);
  return stub;
}

std::expected<HookManager::Target*, ReodeStatus> HookManager::GetOrCreateTarget(
    std::uintptr_t address) {
  for (const auto& target : targets_) {
    if (target->address == address) return target.get();
  }

  auto target = std::make_unique<Target>();
  target->address = address;
  target->trampoline = static_cast<std::uint8_t*>(
      allocator_.Allocate(address, kTrampolineCapacity));
  if (target->trampoline == nullptr) {
    return std::unexpected(kReodeOutOfMemory);
  }
  std::vector<InstructionMapping> mappings;
  auto stolen = BuildTrampoline(address, target->trampoline, &mappings);
  if (!stolen) return std::unexpected(stolen.error());
  FlushInstructionCache(GetCurrentProcess(), target->trampoline,
                        kTrampolineCapacity);

  const auto trampoline = reinterpret_cast<std::uintptr_t>(target->trampoline);
  target->entry_stub = AllocateStub(address, trampoline);
  if (target->entry_stub == nullptr) return std::unexpected(kReodeOutOfMemory);

  std::array<std::uint8_t, kMaxStolenBytes> patch{};
  std::fill(patch.begin(), patch.end(), std::uint8_t{0x90});  // nop
  const auto stub = reinterpret_cast<std::uintptr_t>(target->entry_stub);
  auto relative = static_cast<std::int32_t>(stub - (address + kRel32JumpSize));
  patch[0] = 0xE9;
  std::memcpy(&patch[1], &relative, sizeof(relative));

  {
    ThreadFreezer freezer;
    freezer.RelocateInstructionPointers([&](std::uintptr_t rip) {
      if (rip < address || rip >= address + *stolen) return rip;
      for (const InstructionMapping& mapping : mappings) {
        if (rip == address + mapping.original_offset) {
          return trampoline + mapping.trampoline_offset;
        }
      }
      return rip;
    });
    if (!WriteCode(address, patch.data(), *stolen)) {
      return std::unexpected(kReodeMemoryProtection);
    }
  }

  targets_.push_back(std::move(target));
  return targets_.back().get();
}

void HookManager::RelinkChain(Target& target) {
  std::uintptr_t next = reinterpret_cast<std::uintptr_t>(target.trampoline);
  for (auto it = target.hooks.rbegin(); it != target.hooks.rend(); ++it) {
    Hook& hook = **it;
    hook.next_stub->Set(next);
    if (hook.enabled) next = hook.detour;
  }
  target.entry_stub->Set(next);
}

bool HookManager::OwnsHook(const Hook* hook) const {
  if (hook == nullptr) return false;
  for (const auto& target : targets_) {
    for (const auto& candidate : target->hooks) {
      if (candidate.get() == hook) return true;
    }
  }
  return false;
}

std::expected<HookManager::Hook*, ReodeStatus> HookManager::Create(
    void* target_address, void* detour, std::int32_t priority,
    std::string owner, void** original) {
  if (target_address == nullptr || detour == nullptr || original == nullptr) {
    return std::unexpected(kReodeInvalidArgument);
  }
  std::lock_guard lock(mutex_);
  auto target =
      GetOrCreateTarget(reinterpret_cast<std::uintptr_t>(target_address));
  if (!target) return std::unexpected(target.error());

  const auto detour_address = reinterpret_cast<std::uintptr_t>(detour);
  for (const auto& existing : (*target)->hooks) {
    if (existing->detour == detour_address) {
      return std::unexpected(kReodeAlreadyExists);
    }
  }

  static constinit std::uint64_t next_sequence = 0;
  auto hook = std::make_unique<Hook>();
  hook->target = *target;
  hook->detour = detour_address;
  hook->priority = priority;
  hook->sequence = next_sequence++;
  hook->owner = std::move(owner);
  hook->next_stub =
      AllocateStub((*target)->address,
                   reinterpret_cast<std::uintptr_t>((*target)->trampoline));
  if (hook->next_stub == nullptr) return std::unexpected(kReodeOutOfMemory);

  // The original pointer must be valid before the detour becomes reachable.
  *original = hook->next_stub;

  Hook* result = hook.get();
  auto& hooks = (*target)->hooks;
  auto position = std::find_if(hooks.begin(), hooks.end(),
                               [&](const std::unique_ptr<Hook>& other) {
                                 return other->priority < priority;
                               });
  hooks.insert(position, std::move(hook));
  RelinkChain(**target);
  return result;
}

ReodeStatus HookManager::SetEnabled(Hook* hook, bool enabled) {
  std::lock_guard lock(mutex_);
  if (!OwnsHook(hook)) return kReodeNotFound;
  hook->enabled = enabled;
  RelinkChain(*hook->target);
  return kReodeOk;
}

ReodeStatus HookManager::Remove(Hook* hook) {
  std::lock_guard lock(mutex_);
  if (!OwnsHook(hook)) return kReodeNotFound;
  Target& target = *hook->target;
  // Unlink first; the stub memory itself is never freed, so a thread still
  // running inside the removed detour keeps a valid path to the original.
  hook->enabled = false;
  RelinkChain(target);
  std::erase_if(target.hooks, [hook](const std::unique_ptr<Hook>& candidate) {
    return candidate.get() == hook;
  });
  return kReodeOk;
}

std::vector<HookInfo> HookManager::List() const {
  std::lock_guard lock(mutex_);
  std::vector<HookInfo> result;
  for (const auto& target : targets_) {
    for (const auto& hook : target->hooks) {
      result.push_back({target->address, hook->detour, hook->priority,
                        hook->enabled, hook->owner});
    }
  }
  return result;
}

}  // namespace reode::hook
