// Copyright 2026 The Reode Authors

#include "mod_registry.h"

#include <windows.h>

#include <format>
#include <mutex>
#include <utility>

#include "Geode/loader/Hook.hpp"
#include "Geode/loader/Mod.hpp"
#include "Geode/loader/ModEvent.hpp"
#include "Geode/loader/ModMetadata.hpp"
#include "host.h"

namespace reode::geode_shim {

ModRegistry& ModRegistry::Instance() {
  static ModRegistry* const kInstance = new ModRegistry();
  return *kInstance;
}

geode::Mod* ModRegistry::Begin(ModState state,
                               const geode::ModMetadata& metadata) {
  constructing_ = std::move(state);
  // The constructor calls TakeConstructionState() and Register().
  auto* mod = new geode::Mod(metadata);
  if (auto loaded = mod->loadData(); !loaded) {
    Log(StateOf(mod)->reode_mod, kReodeLogWarning,
        "could not load saved data: " + loaded.unwrapErr());
  }
  current_ = mod;
  return mod;
}

void ModRegistry::SaveAll() {
  for (geode::Mod* mod : All()) {
    if (auto saved = mod->saveData(); !saved) {
      Log(StateOf(mod)->reode_mod, kReodeLogWarning,
          "could not save data: " + saved.unwrapErr());
    }
  }
}

ReodeStatus ModRegistry::Finish(geode::Mod* mod, HMODULE module) {
  ModState* state = StateOf(mod);
  if (state == nullptr) return kReodeInvalidArgument;

  // Every Geode mod exports this; it binds the mod to its geode::Mod
  // (through Loader::takeNextMod) and sets up its async runtime.
  using EntryFn = void (*)();
  auto entry =
      reinterpret_cast<EntryFn>(GetProcAddress(module, "geodeImplicitEntry"));
  if (entry == nullptr) {
    Log(state->reode_mod, kReodeLogError,
        "binary does not export geodeImplicitEntry");
    current_ = nullptr;
    return kReodeIncompatible;
  }
  state->module = module;
  entry();
  current_ = nullptr;
  state->loaded = true;

  std::size_t enabled = 0;
  for (const auto& hook : state->hooks) {
    if (!hook->getAutoEnable()) continue;
    if (auto result = hook->enable(); !result) {
      Log(state->reode_mod, kReodeLogError,
          std::format("Failed to enable hook {}: {}", hook->getDisplayName(),
                      result.unwrapErr()));
    } else {
      ++enabled;
    }
  }
  Log(state->reode_mod, kReodeLogDebug,
      std::format("{} of {} hook(s) enabled", enabled, state->hooks.size()));

  geode::ModStateEvent(geode::ModEventType::DataLoaded, mod).send();
  geode::ModStateEvent(geode::ModEventType::Loaded, mod).send();
  return kReodeOk;
}

void ModRegistry::Abort(geode::Mod* mod) {
  if (current_ == mod) current_ = nullptr;
}

void ModRegistry::Register(geode::Mod* mod, ModState* state) {
  std::lock_guard lock(mutex_);
  states_[mod] = state;
}

ModState* ModRegistry::StateOf(const geode::Mod* mod) const {
  std::lock_guard lock(mutex_);
  auto it = states_.find(mod);
  return it == states_.end() ? nullptr : it->second;
}

geode::Mod* ModRegistry::FindById(std::string_view id) const {
  std::lock_guard lock(mutex_);
  for (const auto& [mod, state] : states_) {
    if (state->id == id) return const_cast<geode::Mod*>(mod);
  }
  return nullptr;
}

std::vector<geode::Mod*> ModRegistry::All() const {
  std::lock_guard lock(mutex_);
  std::vector<geode::Mod*> mods;
  for (const auto& [mod, state] : states_) {
    mods.push_back(const_cast<geode::Mod*>(mod));
  }
  return mods;
}

ModState* ModRegistry::ForCaller(void* return_address) const {
  if (current_ != nullptr) return StateOf(current_);
  HMODULE module = nullptr;
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          static_cast<LPCWSTR>(return_address), &module)) {
    return nullptr;
  }
  std::lock_guard lock(mutex_);
  for (const auto& [mod, state] : states_) {
    if (state->module == module) return state;
  }
  return nullptr;
}

ReodeStatus EnableHook(HookState& hook) {
  if (hook.enabled) return kReodeOk;
  ModState* owner = hook.owner != nullptr
                        ? ModRegistry::Instance().StateOf(hook.owner)
                        : hook.creator;
  if (hook.target == nullptr) hook.target = hook.address;
  ReodeStatus status = HookDispatcher::Instance().Add(
      hook.target, hook.detour, hook.priority, hook.shape,
      owner != nullptr ? owner->reode_mod : nullptr);
  if (status == kReodeOk) hook.enabled = true;
  return status;
}

ReodeStatus DisableHook(HookState& hook) {
  if (!hook.enabled) return kReodeOk;
  ReodeStatus status =
      HookDispatcher::Instance().Remove(hook.target, hook.detour);
  if (status == kReodeOk) hook.enabled = false;
  return status;
}

}  // namespace reode::geode_shim
