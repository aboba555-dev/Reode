// Copyright 2026 The Reode Authors

#ifndef REODE_GEODE_SHIM_SRC_MOD_REGISTRY_H_
#define REODE_GEODE_SHIM_SRC_MOD_REGISTRY_H_

#include <windows.h>

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "hook_dispatcher.h"
#include "reode/api.h"

namespace geode {
class Hook;
class Mod;
class ModMetadata;
class SettingV3;
}  // namespace geode

namespace reode::geode_shim {

// Reode's state for one Geode mod. geode::Mod::Impl derives from it, so
// geode::Mod member functions reach it through m_impl and the rest of the
// shim through ModRegistry::StateOf().
struct ModState {
  std::string id;
  std::string name;
  std::string version;
  std::vector<std::string> developers;
  std::filesystem::path resources_dir;
  std::filesystem::path save_dir;
  ReodeMod* reode_mod = nullptr;
  HMODULE module = nullptr;
  bool loaded = false;
  std::vector<std::pair<std::string, std::shared_ptr<geode::SettingV3>>>
      settings;
  // Hooks claimed before the mod finished loading; enabled on load.
  std::vector<std::shared_ptr<geode::Hook>> hooks;
};

// Reode's state for one geode::Hook (base of geode::Hook::Impl).
struct HookState {
  void* address = nullptr;
  void* detour = nullptr;
  std::string display_name;
  std::string modify_class_name;
  FunctionShape shape;
  std::int32_t priority = 0;
  geode::Mod* owner = nullptr;
  // The mod whose code called Hook::create, if known.
  ModState* creator = nullptr;
  // The address the hook is installed at; set when first enabled.
  void* target = nullptr;
  bool enabled = false;
  bool auto_enable = true;
};

// Owns every geode::Mod and drives the load sequence requested by Reode.
class ModRegistry {
 public:
  static ModRegistry& Instance();

  ModRegistry(const ModRegistry&) = delete;
  ModRegistry& operator=(const ModRegistry&) = delete;

  // Creates the geode::Mod for a mod about to be LoadLibrary'd and makes it
  // the answer to Loader::takeNextMod().
  geode::Mod* Begin(ModState state, const geode::ModMetadata& metadata);
  // Runs geodeImplicitEntry, enables claimed hooks and fires the
  // DataLoaded / Loaded events.
  ReodeStatus Finish(geode::Mod* mod, HMODULE module);
  void Abort(geode::Mod* mod);
  // Writes every mod's save container and settings to disk.
  void SaveAll();

  // The mod currently being loaded (Loader::takeNextMod).
  geode::Mod* current() const { return current_; }

  // Called by the geode::Mod constructor to receive its initial state.
  ModState TakeConstructionState() { return std::move(constructing_); }
  void Register(geode::Mod* mod, ModState* state);

  ModState* StateOf(const geode::Mod* mod) const;
  geode::Mod* FindById(std::string_view id) const;
  std::vector<geode::Mod*> All() const;

  // The mod whose code contains `return_address`: the mod being loaded
  // while its static initializers run, otherwise the owner of the module.
  ModState* ForCaller(void* return_address) const;

 private:
  ModRegistry() = default;
  ~ModRegistry() = default;

  mutable std::mutex mutex_;
  ModState constructing_;
  geode::Mod* current_ = nullptr;
  std::unordered_map<const geode::Mod*, ModState*> states_;
};

// Enables a hook through the dispatcher; shared by Hook::enable() and the
// load sequence.
ReodeStatus EnableHook(HookState& hook);
ReodeStatus DisableHook(HookState& hook);

}  // namespace reode::geode_shim

#endif  // REODE_GEODE_SHIM_SRC_MOD_REGISTRY_H_
