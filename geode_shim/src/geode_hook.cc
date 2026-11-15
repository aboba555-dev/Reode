// Copyright 2026 The Reode Authors
//
// geode::Hook, geode::hook::createConvention and the address helpers used
// by $modify.

#include <intrin.h>
#include <windows.h>

#include <cstdint>
#include <cstring>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "Geode/loader/Hook.hpp"
#include "Geode/loader/Tulip.hpp"
#include "Geode/utils/addresser.hpp"
#include "geode_impl.h"
#include "hook_dispatcher.h"
#include "host.h"
#include "mod_registry.h"
#include "tulip/CallingConvention.hpp"
#include "tulip/WrapperData.hpp"

namespace reode::geode_shim {
namespace {

// Records which Tulip calling convention a Geode hook was declared with.
// Reode generates its own handler code, so the code-generation callbacks
// are never used.
class Convention final : public tulip::hook::CallingConvention {
 public:
  explicit Convention(tulip::hook::TulipConvention kind) : kind_(kind) {}

  void generateIntoDefault(
      tulip::hook::BaseAssembler& /*a*/,
      tulip::hook::AbstractFunction const& /*f*/) override {}
  void generateDefaultCleanup(
      tulip::hook::BaseAssembler& /*a*/,
      tulip::hook::AbstractFunction const& /*f*/) override {}
  void generateIntoOriginal(
      tulip::hook::BaseAssembler& /*a*/,
      tulip::hook::AbstractFunction const& /*f*/) override {}
  void generateOriginalCleanup(
      tulip::hook::BaseAssembler& /*a*/,
      tulip::hook::AbstractFunction const& /*f*/) override {}
  bool needsWrapper(
      tulip::hook::AbstractFunction const& function) const override {
    return IsMemberCall() &&
           function.m_return.m_kind == tulip::hook::AbstractTypeKind::Other;
  }

  bool IsMemberCall() const {
    return kind_ == tulip::hook::TulipConvention::Thiscall ||
           kind_ == tulip::hook::TulipConvention::Membercall;
  }

 private:
  tulip::hook::TulipConvention kind_;
};

// Windows x64 passes four arguments in registers; a class returned by value
// takes one of them as a hidden pointer.
constexpr std::size_t kRegisterArguments = 4;

FunctionShape ShapeOf(const tulip::hook::HandlerMetadata& metadata) {
  const auto& function = metadata.m_abstract;
  const bool returns_class =
      function.m_return.m_kind == tulip::hook::AbstractTypeKind::Other;
  const std::size_t arguments =
      function.m_parameters.size() + (returns_class ? 1 : 0);
  FunctionShape shape;
  shape.stack_argument_count =
      arguments > kRegisterArguments ? arguments - kRegisterArguments : 0;
  const auto* convention =
      dynamic_cast<const Convention*>(metadata.m_convention.get());
  shape.swap_this_and_return =
      convention != nullptr && convention->needsWrapper(function);
  return shape;
}

bool IsInsideModule(std::intptr_t address) {
  HMODULE module = nullptr;
  return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(address),
                            &module) != FALSE;
}

std::int32_t ReadInt32(std::intptr_t address) {
  std::int32_t value = 0;
  std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
  return value;
}

std::string StatusMessage(ReodeStatus status) {
  switch (status) {
    case kReodeOk:
      return "ok";
    case kReodeNotFound:
      return "the function was not found";
    case kReodeAlreadyExists:
      return "the detour is already installed";
    case kReodeUnsupportedInstruction:
      return "the function starts with instructions Reode cannot relocate";
    case kReodeOutOfMemory:
      return "out of memory";
    case kReodeMemoryProtection:
      return "could not change memory protection";
    default:
      return std::format("Reode error {}", static_cast<int>(status));
  }
}

}  // namespace
}  // namespace reode::geode_shim

namespace tulip::hook {

CallingConvention::~CallingConvention() = default;

}  // namespace tulip::hook

namespace geode {

namespace hook {

std::shared_ptr<tulip::hook::CallingConvention> createConvention(
    tulip::hook::TulipConvention convention) noexcept {
  return std::make_shared<reode::geode_shim::Convention>(convention);
}

// Returns a function pointer with the default calling convention for the
// game function at `address`.
Result<void*> createWrapper(
    void* address, tulip::hook::WrapperMetadata const& metadata) noexcept {
  const auto target = reinterpret_cast<std::uintptr_t>(address);
  const auto* convention = dynamic_cast<const reode::geode_shim::Convention*>(
      metadata.m_convention.get());
  if (convention != nullptr && convention->needsWrapper(metadata.m_abstract)) {
    void* thunk = reode::geode_shim::MakeSwapThunk(target);
    if (thunk == nullptr) return Err("out of executable memory");
    return Ok(thunk);
  }
  return Ok(reinterpret_cast<void*>(target));
}

}  // namespace hook

namespace addresser {

// Follows import thunks (jmp [rip+x]) and linker thunks (jmp rel) to the
// real function. Jumps that leave the loaded modules are not followed: they
// belong to hooks, and the hooked function itself is what the caller wants.
std::intptr_t Addresser::followThunkFunction(std::intptr_t address) {
  constexpr int kMaxThunks = 16;
  for (int i = 0; i < kMaxThunks && address != 0; ++i) {
    const auto* code = reinterpret_cast<const std::uint8_t*>(address);
    std::intptr_t next = 0;
    if (code[0] == 0xE9) {
      next = address + 5 + reode::geode_shim::ReadInt32(address + 1);
    } else if (code[0] == 0xEB) {
      next = address + 2 + static_cast<std::int8_t>(code[1]);
    } else if (code[0] == 0xFF && code[1] == 0x25) {
      next = *reinterpret_cast<const std::intptr_t*>(
          address + 6 + reode::geode_shim::ReadInt32(address + 2));
    } else if (code[0] == 0x48 && code[1] == 0xFF && code[2] == 0x25) {
      next = *reinterpret_cast<const std::intptr_t*>(
          address + 7 + reode::geode_shim::ReadInt32(address + 3));
    } else {
      break;
    }
    if (!reode::geode_shim::IsInsideModule(next)) break;
    address = next;
  }
  return address;
}

}  // namespace addresser

Hook::Hook(std::shared_ptr<Impl>&& impl) : m_impl(std::move(impl)) {}

Hook::~Hook() = default;

std::shared_ptr<Hook> Hook::create(void* address, void* detour,
                                   std::string displayName,
                                   tulip::hook::HandlerMetadata handlerMetadata,
                                   tulip::hook::HookMetadata hookMetadata) {
  reode::geode_shim::HookState state;
  state.address = address;
  state.detour = detour;
  state.display_name = std::move(displayName);
  state.shape = reode::geode_shim::ShapeOf(handlerMetadata);
  state.priority = hookMetadata.m_priority;
  state.creator =
      reode::geode_shim::ModRegistry::Instance().ForCaller(_ReturnAddress());
  auto impl = std::make_shared<Impl>(std::move(state));
  // The destructor is private, so the deleter must be created here.
  return std::shared_ptr<Hook>(new Hook(std::move(impl)),
                               [](Hook* hook) { delete hook; });
}

Mod* Hook::getOwner() const { return m_impl->owner; }

bool Hook::isEnabled() const { return m_impl->enabled; }

Result<> Hook::enable() {
  ReodeStatus status = reode::geode_shim::EnableHook(*m_impl);
  if (status != kReodeOk) {
    return Err(reode::geode_shim::StatusMessage(status));
  }
  return Ok();
}

Result<> Hook::disable() {
  ReodeStatus status = reode::geode_shim::DisableHook(*m_impl);
  if (status != kReodeOk) {
    return Err(reode::geode_shim::StatusMessage(status));
  }
  return Ok();
}

Result<> Hook::toggle() { return toggle(!m_impl->enabled); }

Result<> Hook::toggle(bool enable) {
  return enable ? this->enable() : this->disable();
}

bool Hook::getAutoEnable() const { return m_impl->auto_enable; }

void Hook::setAutoEnable(bool autoEnable) { m_impl->auto_enable = autoEnable; }

void Hook::setPriority(int32_t priority) { m_impl->priority = priority; }

int32_t Hook::getPriority() const { return m_impl->priority; }

uintptr_t Hook::getAddress() const {
  return reinterpret_cast<uintptr_t>(
      m_impl->target != nullptr ? m_impl->target : m_impl->address);
}

std::string_view Hook::getDisplayName() const { return m_impl->display_name; }

tulip::hook::HookMetadata Hook::getHookMetadata() const {
  return tulip::hook::HookMetadata{.m_priority = m_impl->priority};
}

void Hook::setHookMetadata(tulip::hook::HookMetadata const& metadata) {
  m_impl->priority = metadata.m_priority;
}

std::optional<std::string_view> Hook::getModifyClassName() const {
  if (m_impl->modify_class_name.empty()) return std::nullopt;
  return m_impl->modify_class_name;
}

void Hook::setModifyClassName(std::string name) {
  m_impl->modify_class_name = std::move(name);
}

}  // namespace geode
