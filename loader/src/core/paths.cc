// Copyright 2026 The Reode Authors

#include "core/paths.h"

#include <windows.h>

#include <filesystem>
#include <string>
#include <system_error>

namespace reode {
namespace {

std::filesystem::path ExecutableDirectory() {
  std::wstring path(MAX_PATH, L'\0');
  for (;;) {
    DWORD length = GetModuleFileNameW(nullptr, path.data(),
                                      static_cast<DWORD>(path.size()));
    if (length < path.size()) {
      path.resize(length);
      break;
    }
    path.resize(path.size() * 2);
  }
  return std::filesystem::path(path).parent_path();
}

Paths* ComputePaths() {
  auto* paths = new Paths();
  paths->game_dir = ExecutableDirectory();
  paths->reode_dir = paths->game_dir / L"reode";
  paths->mods_dir = paths->reode_dir / L"mods";
  paths->unzipped_dir = paths->reode_dir / L"unzipped";
  paths->saves_dir = paths->reode_dir / L"saves";
  paths->logs_dir = paths->reode_dir / L"logs";
  paths->backups_dir = paths->reode_dir / L"backups";
  paths->session_file = paths->reode_dir / L"session.lock";
  paths->compat_dir = paths->reode_dir / L"compat";
  paths->geode_mods_dir = paths->game_dir / L"geode" / L"mods";
  paths->config_file = paths->reode_dir / L"config.json";

  std::error_code error;
  for (const auto* dir :
       {&paths->mods_dir, &paths->unzipped_dir, &paths->saves_dir,
        &paths->logs_dir, &paths->backups_dir, &paths->compat_dir}) {
    std::filesystem::create_directories(*dir, error);
  }
  return paths;
}

}  // namespace

const Paths& GetPaths() {
  static const Paths* const kPaths = ComputePaths();
  return *kPaths;
}

std::string PathToUtf8(const std::filesystem::path& path) {
  std::u8string text = path.u8string();
  return std::string(text.begin(), text.end());
}

}  // namespace reode
