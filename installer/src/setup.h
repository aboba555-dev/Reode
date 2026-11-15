// Copyright 2026 The Reode Authors
//
// What ReodeSetup.exe does to a Geometry Dash directory, without any UI.

#ifndef REODE_INSTALLER_SRC_SETUP_H_
#define REODE_INSTALLER_SRC_SETUP_H_

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace reode::setup {

// Geometry Dash's Steam app id.
inline constexpr char kSteamAppId[] = "322170";

// Library directories listed in Steam's steamapps/libraryfolders.vdf.
std::vector<std::filesystem::path> ParseLibraryFolders(std::string_view vdf);

// Finds the Geometry Dash directory of a Steam install, if any.
std::optional<std::filesystem::path> FindSteamGameDir();

bool IsGameDir(const std::filesystem::path& dir);
bool IsReodeInstalled(const std::filesystem::path& game_dir);
// Whether GeometryDash.exe is running; its files cannot be replaced then.
bool IsGameRunning();

// The files ReodeSetup.exe carries.
struct Payload {
  std::string_view reode_dll;
  std::string_view proxy_dll;  // XInput1_4.dll
  std::string_view geode_dll;  // reode/compat/Geode.dll; empty = not installed
  // Bundled mods for reode/mods; installing replaces older copies.
  struct Mod {
    std::wstring_view file_name;  // e.g. L"reode.clock.reode"
    std::string_view data;
  };
  std::vector<Mod> mods;
};

// Installs or updates Reode. A foreign XInput1_4.dll (Geode's, for example)
// is kept as XInput1_4.dll.reode-backup and restored by Uninstall().
std::expected<void, std::string> Install(const std::filesystem::path& game_dir,
                                         const Payload& payload);

// Removes Reode's files and restores a backed-up XInput1_4.dll. Mods, logs
// and settings in reode/ are deleted only when `remove_data`; the save
// backups in reode/backups are always kept.
std::expected<void, std::string> Uninstall(
    const std::filesystem::path& game_dir, bool remove_data);

}  // namespace reode::setup

#endif  // REODE_INSTALLER_SRC_SETUP_H_
