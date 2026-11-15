// Copyright 2026 The Reode Authors

#ifndef REODE_LOADER_SRC_CORE_PATHS_H_
#define REODE_LOADER_SRC_CORE_PATHS_H_

#include <filesystem>
#include <string>

namespace reode {

// Well-known directories, all derived from the game executable location:
//
//   <game>/reode/            root of everything Reode writes
//   <game>/reode/mods/       *.reode and *.geode packages
//   <game>/reode/unzipped/   extracted packages
//   <game>/reode/saves/      per-mod save directories
//   <game>/reode/logs/       log files
//   <game>/reode/backups/    copies of the game's save files
//   <game>/reode/compat/     Geode compatibility shim (Geode.dll)
//   <game>/geode/mods/       existing Geode packages, also scanned
struct Paths {
  std::filesystem::path game_dir;
  std::filesystem::path reode_dir;
  std::filesystem::path mods_dir;
  std::filesystem::path unzipped_dir;
  std::filesystem::path saves_dir;
  std::filesystem::path logs_dir;
  std::filesystem::path backups_dir;
  // Present while the game runs; see SessionMarker in core/safety.h.
  std::filesystem::path session_file;
  std::filesystem::path compat_dir;
  std::filesystem::path geode_mods_dir;
  std::filesystem::path config_file;
};

// Computes the paths and creates the directories Reode owns.
const Paths& GetPaths();

std::string PathToUtf8(const std::filesystem::path& path);

}  // namespace reode

#endif  // REODE_LOADER_SRC_CORE_PATHS_H_
