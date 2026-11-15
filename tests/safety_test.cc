// Copyright 2026 The Reode Authors

#include "core/safety.h"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace reode {
namespace {

// A fresh, empty directory under the system temp directory, removed again
// when the test ends.
class TempDir {
 public:
  TempDir() {
    path_ = std::filesystem::temp_directory_path() /
            ("reode_test_" + std::to_string(GetCurrentProcessId()) + "_" +
             std::to_string(counter_++));
    std::filesystem::remove_all(path_);
    std::filesystem::create_directories(path_);
  }
  ~TempDir() {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }

  const std::filesystem::path& path() const { return path_; }

 private:
  static inline int counter_ = 0;
  std::filesystem::path path_;
};

void WriteFile(const std::filesystem::path& file, const std::string& text) {
  std::ofstream(file, std::ios::binary) << text;
}

TEST(SafetyTest, KeepsNewestBackupsAndTheFirstOne) {
  std::vector<std::string> deleted = BackupsToDelete(
      {"2026-11-14_10-00-00", kFirstBackupName, "2026-11-12_09-00-00",
       "2026-11-13_08-00-00"},
      2);
  EXPECT_EQ(deleted, std::vector<std::string>{"2026-11-12_09-00-00"});
}

TEST(SafetyTest, DeletesNothingWhenUnderTheLimit) {
  EXPECT_TRUE(BackupsToDelete({"2026-11-14_10-00-00"}, 5).empty());
}

TEST(SafetyTest, CopiesOnlySaveFiles) {
  TempDir saves;
  TempDir backups;
  WriteFile(saves.path() / "CCGameManager.dat", "progress");
  WriteFile(saves.path() / "CCLocalLevels2.dat", "levels");
  WriteFile(saves.path() / "notes.txt", "unrelated");

  auto backup = BackupSaves(saves.path(), backups.path(), "test");
  ASSERT_TRUE(backup.has_value()) << backup.error();
  EXPECT_TRUE(std::filesystem::exists(*backup / "CCGameManager.dat"));
  EXPECT_TRUE(std::filesystem::exists(*backup / "CCLocalLevels2.dat"));
  EXPECT_FALSE(std::filesystem::exists(*backup / "notes.txt"));
}

TEST(SafetyTest, FailsWithoutSaveFiles) {
  TempDir saves;
  TempDir backups;
  EXPECT_FALSE(BackupSaves(saves.path(), backups.path(), "test").has_value());
  EXPECT_FALSE(std::filesystem::exists(backups.path() / "test"));
}

TEST(SafetyTest, SessionMarkerDetectsAnUncleanExit) {
  TempDir dir;
  SessionMarker marker(dir.path() / "session.lock");
  EXPECT_FALSE(marker.PreviousSessionCrashed());
  marker.Start();
  // A new process starting now would see the marker.
  EXPECT_TRUE(marker.PreviousSessionCrashed());
  marker.End();
  EXPECT_FALSE(marker.PreviousSessionCrashed());
}

}  // namespace
}  // namespace reode
