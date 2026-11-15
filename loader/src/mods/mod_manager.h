// Copyright 2026 The Reode Authors

#ifndef REODE_LOADER_SRC_MODS_MOD_MANAGER_H_
#define REODE_LOADER_SRC_MODS_MOD_MANAGER_H_

#include <windows.h>

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/config.h"
#include "mods/mod_metadata.h"
#include "reode/api.h"

namespace reode::mods {
struct LoadedMod;
}  // namespace reode::mods

// Definition of the opaque handle declared in reode/api.h.
struct ReodeMod {
  reode::mods::LoadedMod* mod = nullptr;
};

namespace reode::mods {

enum class ModState { kPending, kLoaded, kDisabled, kFailed };

struct LoadedMod {
  ModMetadata metadata;
  std::string manifest;  // Raw mod.json text.
  std::filesystem::path package;
  std::filesystem::path directory;  // Extracted package contents.
  std::string save_dir;             // UTF-8, handed out to the mod.
  std::string version_text;         // UTF-8, handed out to the mod.
  ModState state = ModState::kPending;
  std::string error;
  HMODULE module = nullptr;
  ReodeMod handle;
};

// Discovers, validates, orders and loads all mods.
class ModManager {
 public:
  static ModManager& Instance();

  ModManager(const ModManager&) = delete;
  ModManager& operator=(const ModManager&) = delete;

  // Loads every mod found in reode/mods (and geode/mods when enabled).
  void LoadAll(const Config& config, const std::string& game_version);

  const std::vector<std::unique_ptr<LoadedMod>>& mods() const { return mods_; }

  // Starts the game without mods; `reason` is shown to the player. Must be
  // called before LoadAll().
  void EnterSafeMode(std::string reason) {
    safe_mode_reason_ = std::move(reason);
  }
  bool safe_mode() const { return !safe_mode_reason_.empty(); }
  const std::string& safe_mode_reason() const { return safe_mode_reason_; }

  // Makes the resources of loaded mods available to cocos2d: adds each
  // mod's resources/ directory to the file search paths and loads its
  // spritesheets. Must run on the main thread once cocos2d is up; safe to
  // call repeatedly (sheets purged by a graphics reload are re-added).
  void LoadResources();

 private:
  ModManager() = default;
  ~ModManager() = default;

  void Discover(const Config& config);
  void AddPackage(const std::filesystem::path& package);
  void ResolveDependencies();
  bool Visit(LoadedMod& mod, std::map<std::string, int>& marks,
             std::vector<LoadedMod*>& order);
  void Load(LoadedMod& mod, const std::string& game_version);
  void LoadReodeMod(LoadedMod& mod, const std::filesystem::path& binary);
  void LoadGeodeMod(LoadedMod& mod, const std::filesystem::path& binary,
                    const std::string& game_version);
  void Fail(LoadedMod& mod, std::string error);
  LoadedMod* Find(const std::string& id);

  Config config_;
  bool search_paths_added_ = false;
  std::string safe_mode_reason_;
  std::vector<std::unique_ptr<LoadedMod>> mods_;
  std::vector<LoadedMod*> load_order_;
};

}  // namespace reode::mods

#endif  // REODE_LOADER_SRC_MODS_MOD_MANAGER_H_
