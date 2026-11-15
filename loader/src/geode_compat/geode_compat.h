// Copyright 2026 The Reode Authors

#ifndef REODE_LOADER_SRC_GEODE_COMPAT_GEODE_COMPAT_H_
#define REODE_LOADER_SRC_GEODE_COMPAT_GEODE_COMPAT_H_

#include <windows.h>

#include <expected>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "mods/mod_metadata.h"
#include "reode/api.h"
#include "reode/geode_shim_host.h"

namespace reode::geode_compat {

// Runs Geode mods on top of Reode.
//
// A Geode mod binary imports its runtime from "Geode.dll" and contains the
// game addresses it hooks, compiled for one game version. Reode ships its
// own Geode.dll (the "shim", see geode_shim/) in reode/compat/, which
// implements the Geode 5.x ABI on top of the Reode API. The shim is loaded
// under that module name before any Geode mod, so the Windows loader binds
// the mods' imports to it.
//
// Before a mod is loaded its import table is compared with the shim's
// export table. A mod is only loaded when every symbol it needs is
// implemented; otherwise it is skipped with a precise list of what is
// missing, instead of failing inside LoadLibrary or crashing later.
class GeodeCompat {
 public:
  struct Assessment {
    bool loadable = false;
    std::string reason;
    // Readable signatures of Geode.dll symbols the shim does not provide.
    std::vector<std::string> missing_symbols;
  };

  explicit GeodeCompat(std::filesystem::path shim_path);

  // Decides whether the Geode mod described by `metadata` with binary
  // `binary` can run under the current game version.
  Assessment Assess(const mods::ModMetadata& metadata,
                    const std::filesystem::path& binary,
                    const std::string& game_version);

  // Loads and initializes the shim once. Must succeed before any Geode mod
  // is loaded.
  std::expected<void, std::string> EnsureShimLoaded(const ReodeApi* api);

  // Loads one Geode mod through the shim's life cycle:
  // BeginMod, LoadLibrary, FinishMod (or AbortMod).
  std::expected<HMODULE, std::string> LoadMod(
      ReodeMod* reode_mod, const std::string& manifest,
      const std::filesystem::path& binary,
      const std::filesystem::path& resources_dir,
      const std::filesystem::path& save_dir);

 private:
  std::filesystem::path shim_path_;
  std::optional<std::set<std::string>> shim_exports_;
  HMODULE shim_module_ = nullptr;
  ReodeGeodeShimBeginModFn begin_mod_ = nullptr;
  ReodeGeodeShimFinishModFn finish_mod_ = nullptr;
  ReodeGeodeShimAbortModFn abort_mod_ = nullptr;
};

}  // namespace reode::geode_compat

#endif  // REODE_LOADER_SRC_GEODE_COMPAT_GEODE_COMPAT_H_
