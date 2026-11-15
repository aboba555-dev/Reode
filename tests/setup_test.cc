// Copyright 2026 The Reode Authors

#include "setup.h"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace reode::setup {
namespace {

namespace fs = std::filesystem;

class FakeGameDir {
 public:
  FakeGameDir() {
    path_ = fs::temp_directory_path() /
            ("reode_setup_test_" + std::to_string(GetCurrentProcessId()) +
             "_" + std::to_string(counter_++));
    fs::remove_all(path_);
    fs::create_directories(path_);
    Write("GeometryDash.exe", "MZ");
  }
  ~FakeGameDir() {
    std::error_code error;
    fs::remove_all(path_, error);
  }

  const fs::path& path() const { return path_; }

  void Write(const fs::path& relative, const std::string& text) const {
    fs::create_directories((path_ / relative).parent_path());
    std::ofstream(path_ / relative, std::ios::binary) << text;
  }
  std::string Read(const fs::path& relative) const {
    std::ifstream stream(path_ / relative, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(stream)),
                       std::istreambuf_iterator<char>());
  }
  bool Exists(const fs::path& relative) const {
    return fs::exists(path_ / relative);
  }

 private:
  static inline int counter_ = 0;
  fs::path path_;
};

const Payload kPayload = {
    .reode_dll = "reode",
    .proxy_dll = "proxy ReodeInitialize",
    .geode_dll = "geode",
    .mods = {{L"reode.clock.reode", "clock"}},
};

TEST(SetupTest, ParsesLibraryFolders) {
  const char* vdf = R"("libraryfolders"
{
	"0"
	{
		"path"		"C:\\Program Files (x86)\\Steam"
		"apps" { "322170"		"123" }
	}
	"1"
	{
		"path"		"D:\\SteamLibrary"
	}
})";
  std::vector<fs::path> folders = ParseLibraryFolders(vdf);
  ASSERT_EQ(folders.size(), 2u);
  EXPECT_EQ(folders[0], fs::path(L"C:\\Program Files (x86)\\Steam"));
  EXPECT_EQ(folders[1], fs::path(L"D:\\SteamLibrary"));
}

TEST(SetupTest, InstallsAndUninstallsCleanly) {
  FakeGameDir game;
  ASSERT_TRUE(Install(game.path(), kPayload).has_value());
  EXPECT_TRUE(IsReodeInstalled(game.path()));
  EXPECT_EQ(game.Read("Reode.dll"), "reode");
  EXPECT_EQ(game.Read("reode/compat/Geode.dll"), "geode");
  EXPECT_EQ(game.Read("reode/mods/reode.clock.reode"), "clock");

  game.Write("reode/mods/some.reode", "mod");
  game.Write("reode/backups/first-launch/CCGameManager.dat", "save");
  ASSERT_TRUE(Uninstall(game.path(), /*remove_data=*/false).has_value());
  EXPECT_FALSE(IsReodeInstalled(game.path()));
  EXPECT_FALSE(game.Exists("XInput1_4.dll"));
  EXPECT_TRUE(game.Exists("reode/mods/some.reode"));
}

TEST(SetupTest, KeepsAndRestoresAForeignProxy) {
  FakeGameDir game;
  game.Write("XInput1_4.dll", "geode loader");
  ASSERT_TRUE(Install(game.path(), kPayload).has_value());
  EXPECT_EQ(game.Read("XInput1_4.dll.reode-backup"), "geode loader");
  // Updating must not overwrite the backup with Reode's own proxy.
  ASSERT_TRUE(Install(game.path(), kPayload).has_value());
  EXPECT_EQ(game.Read("XInput1_4.dll.reode-backup"), "geode loader");

  ASSERT_TRUE(Uninstall(game.path(), /*remove_data=*/false).has_value());
  EXPECT_EQ(game.Read("XInput1_4.dll"), "geode loader");
  EXPECT_FALSE(game.Exists("XInput1_4.dll.reode-backup"));
}

TEST(SetupTest, RemovingDataKeepsSaveBackups) {
  FakeGameDir game;
  ASSERT_TRUE(Install(game.path(), kPayload).has_value());
  game.Write("reode/mods/some.reode", "mod");
  game.Write("reode/backups/first-launch/CCGameManager.dat", "save");
  ASSERT_TRUE(Uninstall(game.path(), /*remove_data=*/true).has_value());
  EXPECT_FALSE(game.Exists("reode/mods"));
  EXPECT_EQ(game.Read("reode/backups/first-launch/CCGameManager.dat"), "save");
}

TEST(SetupTest, RefusesAFolderWithoutTheGame) {
  FakeGameDir game;
  fs::remove(game.path() / "GeometryDash.exe");
  EXPECT_FALSE(Install(game.path(), kPayload).has_value());
}

}  // namespace
}  // namespace reode::setup
