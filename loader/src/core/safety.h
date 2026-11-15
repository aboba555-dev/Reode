// Copyright 2026 The Reode Authors
//
// Protection for the player's progress: backups of the game's save files
// and a safe mode that starts the game without mods after a crash.

#ifndef REODE_LOADER_SRC_CORE_SAFETY_H_
#define REODE_LOADER_SRC_CORE_SAFETY_H_

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace reode {

// Name of the backup made the first time Reode runs. It is never rotated
// out: it holds the saves exactly as they were before any mod ran.
inline constexpr char kFirstBackupName[] = "first-launch";

// Where Geometry Dash keeps its saves: %LOCALAPPDATA%\GeometryDash.
std::filesystem::path GameSaveDirectory();

// Copies the game's save files (CCGameManager*.dat, CCLocalLevels*.dat) from
// `save_dir` into a new subdirectory of `backups_dir`, named `name`. Returns
// that subdirectory. Fails without leaving a partial backup behind.
std::expected<std::filesystem::path, std::string> BackupSaves(
    const std::filesystem::path& save_dir,
    const std::filesystem::path& backups_dir, const std::string& name);

// Of the backup directory names in `names` (timestamps that sort by age,
// see BackupName), returns those to delete so that only the newest `keep`
// remain. The first-launch backup is never returned.
std::vector<std::string> BackupsToDelete(std::vector<std::string> names,
                                         int keep);

// A backup name for the current local time, e.g. "2026-11-14_21-31-35".
std::string BackupName();

// Makes the backups for this start: the first-launch backup if there is
// none yet, then a timestamped one, keeping the newest `keep` timestamped
// backups. `keep` <= 0 disables timestamped backups. Logs what it did.
void BackupSavesOnStartup(const std::filesystem::path& backups_dir, int keep);

// Tracks whether the game closed normally. Start() reports whether the
// previous session ended without calling End(), i.e. the game crashed or
// was killed, and then marks this session as running.
class SessionMarker {
 public:
  explicit SessionMarker(std::filesystem::path file) : file_(std::move(file)) {}

  bool PreviousSessionCrashed() const;
  void Start() const;
  void End() const;

 private:
  std::filesystem::path file_;
};

}  // namespace reode

#endif  // REODE_LOADER_SRC_CORE_SAFETY_H_
