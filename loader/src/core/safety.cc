// Copyright 2026 The Reode Authors

#include "core/safety.h"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "core/logger.h"
#include "core/paths.h"

namespace reode {
namespace {

constexpr const wchar_t* kSaveFilePrefixes[] = {L"CCGameManager",
                                                L"CCLocalLevels"};

bool IsSaveFile(const std::filesystem::path& file) {
  const std::wstring name = file.filename().wstring();
  if (file.extension() != L".dat") return false;
  return std::ranges::any_of(kSaveFilePrefixes, [&](const wchar_t* prefix) {
    return name.starts_with(prefix);
  });
}

}  // namespace

std::filesystem::path GameSaveDirectory() {
  wchar_t buffer[MAX_PATH] = {};
  DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) return {};
  return std::filesystem::path(buffer) / L"GeometryDash";
}

std::expected<std::filesystem::path, std::string> BackupSaves(
    const std::filesystem::path& save_dir,
    const std::filesystem::path& backups_dir, const std::string& name) {
  std::error_code error;
  std::vector<std::filesystem::path> files;
  for (const auto& entry :
       std::filesystem::directory_iterator(save_dir, error)) {
    if (entry.is_regular_file(error) && IsSaveFile(entry.path())) {
      files.push_back(entry.path());
    }
  }
  if (error) {
    return std::unexpected(std::format("cannot read {}: {}",
                                       PathToUtf8(save_dir), error.message()));
  }
  if (files.empty()) {
    return std::unexpected("no save files in " + PathToUtf8(save_dir));
  }

  // Copied into a temporary directory first, so that an interrupted copy
  // never looks like a complete backup.
  const std::filesystem::path target =
      backups_dir / std::filesystem::path(std::u8string(name.begin(),
                                                        name.end()));
  std::filesystem::path partial = target;
  partial += L".partial";
  std::filesystem::remove_all(partial, error);
  std::filesystem::create_directories(partial, error);
  for (const auto& file : files) {
    std::filesystem::copy_file(file, partial / file.filename(),
                               std::filesystem::copy_options::overwrite_existing,
                               error);
    if (error) break;
  }
  if (!error) std::filesystem::rename(partial, target, error);
  if (error) {
    std::error_code ignored;
    std::filesystem::remove_all(partial, ignored);
    return std::unexpected(std::format(
        "cannot write {}: {}", PathToUtf8(target), error.message()));
  }
  return target;
}

std::vector<std::string> BackupsToDelete(std::vector<std::string> names,
                                         int keep) {
  std::erase(names, std::string(kFirstBackupName));
  std::ranges::sort(names);  // Oldest first.
  const std::size_t kept = keep > 0 ? static_cast<std::size_t>(keep) : 0;
  if (names.size() <= kept) return {};
  names.resize(names.size() - kept);
  return names;
}

std::string BackupName() {
  const auto now = std::chrono::zoned_time(
      std::chrono::current_zone(),
      std::chrono::floor<std::chrono::seconds>(
          std::chrono::system_clock::now()));
  return std::format("{:%Y-%m-%d_%H-%M-%S}", now);
}

void BackupSavesOnStartup(const std::filesystem::path& backups_dir,
                          int keep) {
  const std::filesystem::path save_dir = GameSaveDirectory();
  std::error_code error;
  if (save_dir.empty() || !std::filesystem::exists(save_dir, error)) {
    LogInfo("No Geometry Dash saves found yet, nothing to back up");
    return;
  }
  if (!std::filesystem::exists(backups_dir / kFirstBackupName, error)) {
    auto first = BackupSaves(save_dir, backups_dir, kFirstBackupName);
    if (first) {
      LogInfo("Saved the original save files to {}", PathToUtf8(*first));
    } else {
      LogWarning("Could not back up the save files: {}", first.error());
    }
  }
  if (keep <= 0) return;

  auto backup = BackupSaves(save_dir, backups_dir, BackupName());
  if (!backup) {
    LogWarning("Could not back up the save files: {}", backup.error());
    return;
  }
  LogInfo("Save files backed up to {}", PathToUtf8(*backup));

  std::vector<std::string> names;
  for (const auto& entry :
       std::filesystem::directory_iterator(backups_dir, error)) {
    if (entry.is_directory(error) &&
        entry.path().extension() != L".partial") {
      names.push_back(PathToUtf8(entry.path().filename()));
    }
  }
  for (const std::string& name : BackupsToDelete(std::move(names), keep)) {
    std::filesystem::remove_all(
        backups_dir / std::filesystem::path(std::u8string(name.begin(),
                                                          name.end())),
        error);
  }
}

bool SessionMarker::PreviousSessionCrashed() const {
  std::error_code error;
  return std::filesystem::exists(file_, error);
}

void SessionMarker::Start() const {
  std::ofstream stream(file_, std::ios::trunc);
  stream << "Geometry Dash is running. If this file is still here when the "
            "game starts, the previous session did not close normally.\n";
}

void SessionMarker::End() const {
  std::error_code error;
  std::filesystem::remove(file_, error);
}

}  // namespace reode
