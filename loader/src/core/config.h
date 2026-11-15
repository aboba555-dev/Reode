// Copyright 2026 The Reode Authors

#ifndef REODE_LOADER_SRC_CORE_CONFIG_H_
#define REODE_LOADER_SRC_CORE_CONFIG_H_

#include <filesystem>
#include <set>
#include <string>

#include "core/logger.h"

namespace reode {

// User configuration stored in reode/config.json. Missing keys keep their
// defaults, and the file is rewritten with every known key on startup so
// users can discover the options.
struct Config {
  bool show_console = true;
  LogSeverity log_level = LogSeverity::kDebug;
  // Overrides game version detection, e.g. "2.2081". Empty = auto-detect.
  std::string game_version;
  bool load_geode_mods = true;
  std::set<std::string> disabled_mods;
  // Number of timestamped save backups to keep in reode/backups, one made
  // per game start. 0 disables them; the first-launch backup is always made.
  int save_backups = 5;
  // Start without mods when the previous session did not close normally.
  bool safe_mode_after_crash = true;
};

Config LoadConfig(const std::filesystem::path& file);
void SaveConfig(const std::filesystem::path& file, const Config& config);

}  // namespace reode

#endif  // REODE_LOADER_SRC_CORE_CONFIG_H_
