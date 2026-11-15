// Copyright 2026 The Reode Authors
//
// Header-only C++23 SDK for Reode mods.
//
// Example:
//
//   #include <reode/reode.h>
//
//   namespace {
//
//   constinit bool (*g_menu_layer_init)(void*) = nullptr;
//
//   bool MenuLayerInit(void* self) {
//     if (!g_menu_layer_init(self)) return false;
//     reode::Mod::Get().Info("Main menu opened");
//     return true;
//   }
//
//   }  // namespace
//
//   REODE_MOD_MAIN(mod) {
//     mod.Info("Hello from {}", mod.id());
//     auto target = reode::game::FindVirtual("MenuLayer", 9);
//     if (!target) return std::unexpected(target.error());
//     auto hook = mod.Hook(*target, &MenuLayerInit, &g_menu_layer_init);
//     if (!hook) return std::unexpected(hook.error());
//     return {};
//   }

#ifndef REODE_LOADER_INCLUDE_REODE_REODE_H_
#define REODE_LOADER_INCLUDE_REODE_REODE_H_

#include <cstdint>
#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "reode/api.h"

namespace reode {

enum class Status : std::int32_t {
  kOk = kReodeOk,
  kInvalidArgument = kReodeInvalidArgument,
  kNotFound = kReodeNotFound,
  kAlreadyExists = kReodeAlreadyExists,
  kUnsupportedInstruction = kReodeUnsupportedInstruction,
  kOutOfMemory = kReodeOutOfMemory,
  kMemoryProtection = kReodeMemoryProtection,
  kIncompatible = kReodeIncompatible,
  kInternalError = kReodeInternalError,
};

enum class LogLevel : std::int32_t {
  kDebug = kReodeLogDebug,
  kInfo = kReodeLogInfo,
  kWarning = kReodeLogWarning,
  kError = kReodeLogError,
};

template <typename T>
using Result = std::expected<T, Status>;

// Returns a short English description of `status`.
constexpr std::string_view StatusToString(Status status) {
  switch (status) {
    case Status::kOk:
      return "ok";
    case Status::kInvalidArgument:
      return "invalid argument";
    case Status::kNotFound:
      return "not found";
    case Status::kAlreadyExists:
      return "already exists";
    case Status::kUnsupportedInstruction:
      return "unsupported instruction in hooked function";
    case Status::kOutOfMemory:
      return "out of memory";
    case Status::kMemoryProtection:
      return "memory protection failure";
    case Status::kIncompatible:
      return "incompatible";
    case Status::kInternalError:
      return "internal error";
  }
  return "unknown status";
}

namespace internal {

// Set once by REODE_MOD_MAIN before the user's entry point runs.
inline constinit const ReodeApi* g_api = nullptr;
inline constinit ReodeMod* g_mod = nullptr;

inline Result<void> ToResult(ReodeStatus status) {
  if (status == kReodeOk) return {};
  return std::unexpected(static_cast<Status>(status));
}

template <typename Fn>
concept FunctionPointer =
    std::is_pointer_v<Fn> && std::is_function_v<std::remove_pointer_t<Fn>>;

}  // namespace internal

// A hook created through Mod::Hook(). This is a non-owning handle: hooks stay
// installed for the lifetime of the process unless Remove() is called.
class HookHandle {
 public:
  HookHandle() = default;
  explicit HookHandle(ReodeHook* hook) : hook_(hook) {}

  Result<void> Enable() const {
    return internal::ToResult(internal::g_api->set_hook_enabled(hook_, 1));
  }
  Result<void> Disable() const {
    return internal::ToResult(internal::g_api->set_hook_enabled(hook_, 0));
  }
  Result<void> Remove() const {
    return internal::ToResult(internal::g_api->remove_hook(hook_));
  }

  ReodeHook* raw() const { return hook_; }

 private:
  ReodeHook* hook_ = nullptr;
};

// The currently running mod.
class Mod {
 public:
  // Returns the mod this code belongs to. Only valid once REODE_MOD_MAIN ran.
  static Mod& Get() {
    static constinit Mod mod;
    return mod;
  }

  std::string_view id() const { return api().mod_id(internal::g_mod); }
  std::string_view version() const {
    return api().mod_version(internal::g_mod);
  }
  std::string_view save_dir() const {
    return api().mod_save_dir(internal::g_mod);
  }

  void Log(LogLevel level, std::string_view message) const {
    std::string text(message);
    api().log(internal::g_mod, static_cast<ReodeLogLevel>(level), text.c_str());
  }

  template <typename... Args>
  void Debug(std::format_string<Args...> format, Args&&... args) const {
    Log(LogLevel::kDebug, std::format(format, std::forward<Args>(args)...));
  }
  template <typename... Args>
  void Info(std::format_string<Args...> format, Args&&... args) const {
    Log(LogLevel::kInfo, std::format(format, std::forward<Args>(args)...));
  }
  template <typename... Args>
  void Warn(std::format_string<Args...> format, Args&&... args) const {
    Log(LogLevel::kWarning, std::format(format, std::forward<Args>(args)...));
  }
  template <typename... Args>
  void Error(std::format_string<Args...> format, Args&&... args) const {
    Log(LogLevel::kError, std::format(format, std::forward<Args>(args)...));
  }

  // Hooks `target`. The x64 Windows calling convention passes `this` as the
  // first argument, so member functions are hooked with free functions that
  // take the object pointer first. `original` must outlive the hook; a
  // namespace-scope `constinit` pointer is the usual choice.
  template <internal::FunctionPointer Fn>
  Result<HookHandle> Hook(void* target, Fn detour, Fn* original,
                          std::int32_t priority = 0) const {
    ReodeHook* hook = nullptr;
    auto status = api().create_hook(internal::g_mod, target,
                                    reinterpret_cast<void*>(detour), priority,
                                    reinterpret_cast<void**>(original), &hook);
    if (status != kReodeOk) {
      return std::unexpected(static_cast<Status>(status));
    }
    return HookHandle(hook);
  }

 private:
  constexpr Mod() = default;

  static const ReodeApi& api() { return *internal::g_api; }
};

// Helpers for locating game code.
namespace game {

inline std::uintptr_t Base() {
  return reinterpret_cast<std::uintptr_t>(internal::g_api->game_base());
}

inline std::string_view Version() { return internal::g_api->game_version(); }

inline Result<void*> FindVTable(const char* class_name,
                                std::uint32_t subobject_offset = 0) {
  void* vtable = internal::g_api->find_vtable(class_name, subobject_offset);
  if (vtable == nullptr) return std::unexpected(Status::kNotFound);
  return vtable;
}

inline Result<void*> FindVirtual(const char* class_name, std::uint32_t slot) {
  void* function = internal::g_api->find_virtual(class_name, slot);
  if (function == nullptr) return std::unexpected(Status::kNotFound);
  return function;
}

inline Result<void*> FindPattern(const char* pattern) {
  void* address = internal::g_api->find_pattern(pattern);
  if (address == nullptr) return std::unexpected(Status::kNotFound);
  return address;
}

}  // namespace game
}  // namespace reode

// Defines the entry point of a mod. The body receives `reode::Mod& name` and
// returns reode::Result<void>.
#define REODE_MOD_MAIN(name)                                             \
  static ::reode::Result<void> ReodeUserMain(::reode::Mod& name);        \
  extern "C" __declspec(dllexport) ReodeStatus ReodeModLoad(             \
      const ReodeApi* api, ReodeMod* mod) {                              \
    if (api == nullptr || api->version != REODE_API_VERSION ||           \
        api->size < sizeof(ReodeApi)) {                                  \
      return kReodeIncompatible;                                         \
    }                                                                    \
    ::reode::internal::g_api = api;                                      \
    ::reode::internal::g_mod = mod;                                      \
    auto result = ReodeUserMain(::reode::Mod::Get());                    \
    return result ? kReodeOk : static_cast<ReodeStatus>(result.error()); \
  }                                                                      \
  static ::reode::Result<void> ReodeUserMain(::reode::Mod& name)

#endif  // REODE_LOADER_INCLUDE_REODE_REODE_H_
