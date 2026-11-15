// Copyright 2026 The Reode Authors

#include "hook_dispatcher.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <format>
#include <initializer_list>
#include <limits>
#include <memory>
#include <mutex>
#include <vector>

#include "host.h"

namespace reode::geode_shim {
namespace {

// Number of distinct hooked addresses; bounded by the per-thread depth
// table below.
constexpr std::size_t kMaxTargets = 4096;
constexpr std::size_t kCodeBlockSize = 4096;
constexpr std::int32_t kShadowSpace = 0x20;
// Locals: xmm0-3 argument saves (0x40), rax and xmm0 result saves (0x20).
constexpr std::int32_t kLocalsSize = 0x60;

// Per-thread chain depth for every target. Trivially destructible, so it is
// safe in a dynamically loaded DLL.
thread_local std::array<std::uint16_t, kMaxTargets> t_depth{};

// Minimal x86-64 emitter for the handler. Memory operands are always
// [rbp + disp32].
class Emitter {
 public:
  explicit Emitter(std::uint8_t* buffer) : buffer_(buffer) {}

  void Bytes(std::initializer_list<std::uint8_t> bytes) {
    for (std::uint8_t byte : bytes) buffer_[size_++] = byte;
  }
  void Int32(std::int32_t value) {
    std::memcpy(buffer_ + size_, &value, sizeof(value));
    size_ += sizeof(value);
  }
  void Int64(std::uint64_t value) {
    std::memcpy(buffer_ + size_, &value, sizeof(value));
    size_ += sizeof(value);
  }

  // mov [rbp+disp], reg / mov reg, [rbp+disp] for rcx, rdx, r8, r9, rax.
  enum class Gpr { kRax, kRcx, kRdx, kR8, kR9 };
  void Store(Gpr reg, std::int32_t disp) { Move(reg, disp, 0x89); }
  void Load(Gpr reg, std::int32_t disp) { Move(reg, disp, 0x8B); }

  // movups [rbp+disp], xmmN / movups xmmN, [rbp+disp]
  void StoreXmm(int xmm, std::int32_t disp) { MoveXmm(xmm, disp, 0x11); }
  void LoadXmm(int xmm, std::int32_t disp) { MoveXmm(xmm, disp, 0x10); }

  void MovRcxImm(std::uint64_t value) {
    Bytes({0x48, 0xB9});
    Int64(value);
  }
  void MovRaxImm(std::uint64_t value) {
    Bytes({0x48, 0xB8});
    Int64(value);
  }

  void Align(std::size_t alignment) {
    while (size_ % alignment != 0) Bytes({0xCC});
  }

  std::size_t size() const { return size_; }
  std::uint8_t* at(std::size_t offset) const { return buffer_ + offset; }

 private:
  void Move(Gpr reg, std::int32_t disp, std::uint8_t opcode) {
    static constexpr std::uint8_t kRex[] = {0x48, 0x48, 0x48, 0x4C, 0x4C};
    static constexpr std::uint8_t kModRm[] = {0x85, 0x8D, 0x95, 0x85, 0x8D};
    const auto index = static_cast<std::size_t>(reg);
    Bytes({kRex[index], opcode, kModRm[index]});
    Int32(disp);
  }
  void MoveXmm(int xmm, std::int32_t disp, std::uint8_t opcode) {
    Bytes({0x0F, opcode, static_cast<std::uint8_t>(0x85 | (xmm << 3))});
    Int32(disp);
  }

  std::uint8_t* buffer_;
  std::size_t size_ = 0;
};

}  // namespace

class HookTarget {
 public:
  struct Entry {
    void* detour = nullptr;
    std::int32_t priority = 0;
  };

  std::uintptr_t address = 0;
  std::size_t index = 0;
  FunctionShape shape;
  // Enabled detours in call order. Published copies are never freed, so a
  // thread that loaded an old chain can keep using it.
  std::atomic<const std::vector<void*>*> chain{nullptr};
  std::vector<Entry> entries;  // Guarded by HookDispatcher::mutex_.
  // Written by Reode before the hook becomes reachable.
  void* original = nullptr;
  // Jumps to `original`, converting the calling convention back if needed.
  void* original_entry = nullptr;
  void* handler = nullptr;
  ReodeHook* reode_hook = nullptr;
  ReodeMod* owner = nullptr;
  // Set on the first call, for a one-time diagnostic log line.
  std::atomic<bool> called{false};

  void Publish() {
    auto* next = new std::vector<void*>();
    for (const Entry& entry : entries) next->push_back(entry.detour);
    chain.store(next, std::memory_order_release);
  }
};

namespace {

// Called by the generated handler on entry: returns the function to run
// next for the current thread.
void* EnterHook(HookTarget* target) {
  if (!target->called.exchange(true, std::memory_order_relaxed)) {
    const auto base =
        reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    Log(target->owner, kReodeLogDebug,
        std::format("hooked function {:#x} called for the first time",
                    target->address >= base ? target->address - base
                                            : target->address));
  }
  const std::vector<void*>* chain =
      target->chain.load(std::memory_order_acquire);
  const std::size_t depth = t_depth[target->index]++;
  if (chain != nullptr && depth < chain->size()) return (*chain)[depth];
  return target->original_entry;
}

// Called by the generated handler after the function returned.
void LeaveHook(HookTarget* target) { --t_depth[target->index]; }

// Generates the handler, the original-function thunk and their unwind data
// into a fresh executable block.
bool GenerateCode(HookTarget* target) {
  auto* block = static_cast<std::uint8_t*>(
      VirtualAlloc(nullptr, kCodeBlockSize, MEM_RESERVE | MEM_COMMIT,
                   PAGE_EXECUTE_READWRITE));
  if (block == nullptr) return false;

  const auto stack_args =
      static_cast<std::int32_t>(target->shape.stack_argument_count * 8);
  const std::int32_t stack_area = (stack_args + 15) & ~15;
  const std::int32_t locals = kShadowSpace + stack_area;
  const std::int32_t frame = locals + kLocalsSize;  // Multiple of 16.
  // After `mov rbp, rsp`: caller's home space and stack arguments.
  const std::int32_t home = frame + 0x10;
  const std::int32_t caller_stack_args = frame + 0x30;
  const std::int32_t xmm_saves = locals;
  const std::int32_t rax_save = locals + 0x40;
  const std::int32_t xmm0_result = locals + 0x50;

  using Gpr = Emitter::Gpr;
  Emitter e(block);

  // Prolog (described by the unwind data below).
  e.Bytes({0x55});  // push rbp
  const auto prolog_push = static_cast<std::uint8_t>(e.size());
  e.Bytes({0x48, 0x81, 0xEC});  // sub rsp, frame
  e.Int32(frame);
  const auto prolog_alloc = static_cast<std::uint8_t>(e.size());
  e.Bytes({0x48, 0x89, 0xE5});  // mov rbp, rsp
  const auto prolog_end = static_cast<std::uint8_t>(e.size());

  // Save all argument registers.
  e.Store(Gpr::kRcx, home);
  e.Store(Gpr::kRdx, home + 0x08);
  e.Store(Gpr::kR8, home + 0x10);
  e.Store(Gpr::kR9, home + 0x18);
  for (int i = 0; i < 4; ++i) e.StoreXmm(i, xmm_saves + i * 0x10);

  // rax = EnterHook(target)
  e.MovRcxImm(reinterpret_cast<std::uint64_t>(target));
  e.MovRaxImm(reinterpret_cast<std::uint64_t>(&EnterHook));
  e.Bytes({0xFF, 0xD0});        // call rax
  e.Bytes({0x49, 0x89, 0xC3});  // mov r11, rax

  // Restore the arguments and copy stack arguments into our outgoing area.
  for (int i = 0; i < 4; ++i) e.LoadXmm(i, xmm_saves + i * 0x10);
  for (std::int32_t offset = 0; offset < stack_args; offset += 8) {
    e.Load(Gpr::kRax, caller_stack_args + offset);
    e.Store(Gpr::kRax, kShadowSpace + offset);
  }
  e.Load(Gpr::kRcx, home);
  e.Load(Gpr::kRdx, home + 0x08);
  e.Load(Gpr::kR8, home + 0x10);
  e.Load(Gpr::kR9, home + 0x18);
  if (target->shape.swap_this_and_return) {
    e.Bytes({0x48, 0x87, 0xCA});  // xchg rcx, rdx
  }
  e.Bytes({0x41, 0xFF, 0xD3});  // call r11

  // LeaveHook(target), preserving both possible return registers.
  e.Store(Gpr::kRax, rax_save);
  e.StoreXmm(0, xmm0_result);
  e.MovRcxImm(reinterpret_cast<std::uint64_t>(target));
  e.MovRaxImm(reinterpret_cast<std::uint64_t>(&LeaveHook));
  e.Bytes({0xFF, 0xD0});  // call rax
  e.Load(Gpr::kRax, rax_save);
  e.LoadXmm(0, xmm0_result);

  // Epilog.
  e.Bytes({0x48, 0x81, 0xC4});  // add rsp, frame
  e.Int32(frame);
  e.Bytes({0x5D, 0xC3});  // pop rbp; ret
  const std::size_t handler_end = e.size();

  // Original thunk: undo the argument swap, then jump through the pointer
  // Reode wrote into target->original.
  e.Align(16);
  const std::size_t thunk = e.size();
  if (target->shape.swap_this_and_return) {
    e.Bytes({0x48, 0x87, 0xCA});  // xchg rcx, rdx
  }
  e.MovRaxImm(reinterpret_cast<std::uint64_t>(&target->original));
  e.Bytes({0xFF, 0x20});  // jmp [rax]

  // UNWIND_INFO for the handler: push rbp; sub rsp, frame; mov rbp, rsp.
  e.Align(4);
  const std::size_t unwind = e.size();
  std::vector<std::uint16_t> codes;
  auto code = [](std::uint8_t offset, std::uint8_t op, std::uint8_t info) {
    return static_cast<std::uint16_t>(offset | (op << 8) | (info << 12));
  };
  constexpr std::uint8_t kPushNonvol = 0;
  constexpr std::uint8_t kAllocLarge = 1;
  constexpr std::uint8_t kAllocSmall = 2;
  constexpr std::uint8_t kSetFpreg = 3;
  constexpr std::uint8_t kRbp = 5;
  codes.push_back(code(prolog_end, kSetFpreg, 0));
  if (frame <= 128) {
    codes.push_back(code(prolog_alloc, kAllocSmall,
                         static_cast<std::uint8_t>(frame / 8 - 1)));
  } else {
    codes.push_back(code(prolog_alloc, kAllocLarge, 0));
    codes.push_back(static_cast<std::uint16_t>(frame / 8));
  }
  codes.push_back(code(prolog_push, kPushNonvol, kRbp));
  e.Bytes({0x01, prolog_end, static_cast<std::uint8_t>(codes.size()),
           kRbp});  // Version 1, frame register rbp, offset 0.
  for (std::uint16_t value : codes) {
    e.Bytes({static_cast<std::uint8_t>(value & 0xFF),
             static_cast<std::uint8_t>(value >> 8)});
  }
  if (codes.size() % 2 != 0) e.Bytes({0, 0});

  e.Align(4);
  auto* function = reinterpret_cast<RUNTIME_FUNCTION*>(e.at(e.size()));
  function->BeginAddress = 0;
  function->EndAddress = static_cast<DWORD>(handler_end);
  function->UnwindData = static_cast<DWORD>(unwind);
  RtlAddFunctionTable(function, 1, reinterpret_cast<DWORD64>(block));

  FlushInstructionCache(GetCurrentProcess(), block, kCodeBlockSize);
  target->handler = block;
  target->original_entry = e.at(thunk);
  return true;
}

// Bump allocator for small executable stubs; never freed.
void* AllocateStub(std::size_t size) {
  static std::mutex mutex;
  static std::uint8_t* block = nullptr;
  static std::size_t used = kCodeBlockSize;
  std::lock_guard lock(mutex);
  size = (size + 15) & ~std::size_t{15};
  if (used + size > kCodeBlockSize) {
    block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, kCodeBlockSize,
                                                    MEM_RESERVE | MEM_COMMIT,
                                                    PAGE_EXECUTE_READWRITE));
    if (block == nullptr) return nullptr;
    used = 0;
  }
  void* result = block + used;
  used += size;
  return result;
}

}  // namespace

void* MakeSwapThunk(std::uintptr_t target) {
  auto* code = static_cast<std::uint8_t*>(AllocateStub(16));
  if (code == nullptr) return nullptr;
  Emitter e(code);
  e.Bytes({0x48, 0x87, 0xCA});  // xchg rcx, rdx
  e.MovRaxImm(target);
  e.Bytes({0xFF, 0xE0});  // jmp rax
  FlushInstructionCache(GetCurrentProcess(), code, e.size());
  return code;
}

HookDispatcher& HookDispatcher::Instance() {
  static HookDispatcher* const kInstance = new HookDispatcher();
  return *kInstance;
}

HookTarget* HookDispatcher::FindOrCreate(void* address,
                                         const FunctionShape& shape,
                                         ReodeMod* owner, ReodeStatus* status) {
  const auto value = reinterpret_cast<std::uintptr_t>(address);
  for (const auto& target : targets_) {
    if (target->address == value) return target.get();
  }
  if (targets_.size() >= kMaxTargets) {
    *status = kReodeOutOfMemory;
    return nullptr;
  }
  auto target = std::make_unique<HookTarget>();
  target->address = value;
  target->index = targets_.size();
  target->shape = shape;
  target->owner = owner;
  target->Publish();
  if (!GenerateCode(target.get())) {
    *status = kReodeOutOfMemory;
    return nullptr;
  }
  // Geode detours call the original by calling `address` again, which
  // re-enters the Reode chain. The handler must therefore run before every
  // Reode hook, so that native Reode hooks run exactly once, after the
  // whole Geode chain.
  *status = Api().create_hook(owner, address, target->handler,
                              std::numeric_limits<std::int32_t>::max(),
                              &target->original, &target->reode_hook);
  if (*status != kReodeOk) return nullptr;
  targets_.push_back(std::move(target));
  return targets_.back().get();
}

ReodeStatus HookDispatcher::Add(void* address, void* detour,
                                std::int32_t priority,
                                const FunctionShape& shape, ReodeMod* owner) {
  if (address == nullptr || detour == nullptr) return kReodeInvalidArgument;
  std::lock_guard lock(mutex_);
  ReodeStatus status = kReodeOk;
  HookTarget* target = FindOrCreate(address, shape, owner, &status);
  if (target == nullptr) return status;
  for (const HookTarget::Entry& entry : target->entries) {
    if (entry.detour == detour) return kReodeAlreadyExists;
  }
  auto position = std::find_if(target->entries.begin(), target->entries.end(),
                               [&](const HookTarget::Entry& entry) {
                                 return entry.priority > priority;
                               });
  target->entries.insert(position, {detour, priority});
  target->Publish();
  return kReodeOk;
}

ReodeStatus HookDispatcher::Remove(void* address, void* detour) {
  std::lock_guard lock(mutex_);
  const auto value = reinterpret_cast<std::uintptr_t>(address);
  for (const auto& target : targets_) {
    if (target->address != value) continue;
    auto removed = std::erase_if(
        target->entries,
        [&](const HookTarget::Entry& entry) { return entry.detour == detour; });
    if (removed == 0) return kReodeNotFound;
    target->Publish();
    return kReodeOk;
  }
  return kReodeNotFound;
}

}  // namespace reode::geode_shim
