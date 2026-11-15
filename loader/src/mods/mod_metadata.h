// Copyright 2026 The Reode Authors

#ifndef REODE_LOADER_SRC_MODS_MOD_METADATA_H_
#define REODE_LOADER_SRC_MODS_MOD_METADATA_H_

#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include "mods/version.h"

namespace reode::mods {

enum class ModFormat {
  kReode,  // Native Reode mod: exports ReodeModLoad, uses reode/api.h.
  kGeode,  // Geode mod: imports Geode.dll, runs through the compat layer.
};

struct Dependency {
  std::string id;
  VersionRequirement requirement;
  bool required = true;
};

// Parsed mod.json. Both the Reode schema and the Geode schema are accepted;
// the presence of a top-level "reode" or "geode" key selects the format.
//
// Reode mod.json:
//   {
//     "reode": "1",                      // REODE_API_VERSION it targets
//     "id": "author.mod_name",
//     "name": "Mod Name",
//     "version": "1.0.0",
//     "developers": ["Author"],
//     "description": "...",
//     "gd": "2.2081",                    // optional
//     "dependencies": [{"id": "x.y", "version": ">=1.0.0"}],
//     "binary": "author.mod_name.dll"    // optional
//   }
struct ModMetadata {
  ModFormat format = ModFormat::kReode;
  std::string id;
  std::string name;
  Version version;
  std::vector<std::string> developers;
  std::string description;
  // Windows Geometry Dash version the mod was built for, empty = any.
  std::string game_version;
  // API version string from the "reode" / "geode" key.
  std::string loader_version;
  std::vector<Dependency> dependencies;
  // DLL file name inside the package.
  std::string binary;
  // Names from "resources": {"spritesheets": {...}}. The packaged sheets
  // live in resources/<id>/<name>.plist (plus -hd / -uhd variants).
  std::vector<std::string> spritesheets;
};

// Mod ids are used as directory names, so they are restricted to
// lowercase ASCII letters, digits, '.', '_' and '-'.
bool IsValidModId(std::string_view id);

std::expected<ModMetadata, std::string> ParseModMetadata(
    std::string_view json_text);

}  // namespace reode::mods

#endif  // REODE_LOADER_SRC_MODS_MOD_METADATA_H_
