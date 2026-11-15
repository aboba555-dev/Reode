// Copyright 2026 The Reode Authors

#include "setup.h"

#include <windows.h>
#include <tlhelp32.h>

#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace reode::setup {
namespace {

namespace fs = std::filesystem;

constexpr wchar_t kGameExe[] = L"GeometryDash.exe";
constexpr wchar_t kProxyName[] = L"XInput1_4.dll";
constexpr wchar_t kProxyBackupName[] = L"XInput1_4.dll.reode-backup";
constexpr wchar_t kReodeDllName[] = L"Reode.dll";
// Exported by Reode's proxy only; tells it apart from other XInput DLLs.
constexpr std::string_view kProxyMarker = "ReodeInitialize";

std::string Narrow(const fs::path& path) {
  std::u8string text = path.u8string();
  return std::string(text.begin(), text.end());
}

std::optional<std::string> ReadFile(const fs::path& file) {
  std::ifstream stream(file, std::ios::binary);
  if (!stream) return std::nullopt;
  return std::string((std::istreambuf_iterator<char>(stream)),
                     std::istreambuf_iterator<char>());
}

std::optional<fs::path> RegistryPath(HKEY root, const wchar_t* key,
                                     const wchar_t* value) {
  wchar_t buffer[MAX_PATH] = {};
  DWORD size = sizeof(buffer);
  if (RegGetValueW(root, key, value, RRF_RT_REG_SZ, nullptr, buffer, &size) !=
      ERROR_SUCCESS) {
    return std::nullopt;
  }
  return fs::path(buffer).make_preferred();
}

bool IsReodeProxy(const fs::path& file) {
  auto bytes = ReadFile(file);
  return bytes && bytes->find(kProxyMarker) != std::string::npos;
}

// Writes next to the target, then swaps it in, so that a failed write never
// leaves a truncated DLL behind.
std::expected<void, std::string> WriteFileAtomically(const fs::path& file,
                                                     std::string_view data) {
  std::error_code error;
  fs::create_directories(file.parent_path(), error);
  fs::path temp = file;
  temp += L".new";
  {
    std::ofstream stream(temp, std::ios::binary | std::ios::trunc);
    stream.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!stream) return std::unexpected("cannot write " + Narrow(temp));
  }
  fs::rename(temp, file, error);
  if (error) {
    fs::remove(temp, error);
    return std::unexpected(
        std::format("cannot replace {}: {}", Narrow(file), error.message()));
  }
  return {};
}

}  // namespace

std::vector<fs::path> ParseLibraryFolders(std::string_view vdf) {
  // Entries look like:  "path"		"D:\\SteamLibrary"
  std::vector<fs::path> folders;
  constexpr std::string_view kKey = "\"path\"";
  std::size_t pos = 0;
  while ((pos = vdf.find(kKey, pos)) != std::string_view::npos) {
    pos += kKey.size();
    std::size_t open = vdf.find('"', pos);
    if (open == std::string_view::npos) break;
    std::string value;
    std::size_t i = open + 1;
    for (; i < vdf.size() && vdf[i] != '"'; ++i) {
      if (vdf[i] == '\\' && i + 1 < vdf.size()) ++i;  // \\ and \" escapes.
      value += vdf[i];
    }
    pos = i;
    if (!value.empty()) {
      folders.emplace_back(std::u8string(value.begin(), value.end()));
    }
  }
  return folders;
}

std::optional<fs::path> FindSteamGameDir() {
  std::optional<fs::path> steam =
      RegistryPath(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath");
  if (!steam) {
    steam = RegistryPath(HKEY_LOCAL_MACHINE,
                         L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath");
  }
  if (!steam) return std::nullopt;

  std::vector<fs::path> libraries = {*steam};
  if (auto vdf = ReadFile(*steam / L"steamapps" / L"libraryfolders.vdf")) {
    for (fs::path& library : ParseLibraryFolders(*vdf)) {
      libraries.push_back(std::move(library));
    }
  }
  for (const fs::path& library : libraries) {
    fs::path dir = library / L"steamapps" / L"common" / L"Geometry Dash";
    if (IsGameDir(dir)) return dir;
  }
  return std::nullopt;
}

bool IsGameDir(const fs::path& dir) {
  std::error_code error;
  return fs::is_regular_file(dir / kGameExe, error);
}

bool IsReodeInstalled(const fs::path& game_dir) {
  std::error_code error;
  return fs::exists(game_dir / kReodeDllName, error) ||
         IsReodeProxy(game_dir / kProxyName);
}

bool IsGameRunning() {
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) return false;
  PROCESSENTRY32W entry = {.dwSize = sizeof(entry)};
  bool running = false;
  for (BOOL ok = Process32FirstW(snapshot, &entry); ok && !running;
       ok = Process32NextW(snapshot, &entry)) {
    running = _wcsicmp(entry.szExeFile, kGameExe) == 0;
  }
  CloseHandle(snapshot);
  return running;
}

std::expected<void, std::string> Install(const fs::path& game_dir,
                                         const Payload& payload) {
  if (!IsGameDir(game_dir)) {
    return std::unexpected("GeometryDash.exe is not in " + Narrow(game_dir));
  }
  const fs::path proxy = game_dir / kProxyName;
  const fs::path backup = game_dir / kProxyBackupName;
  std::error_code error;
  if (fs::exists(proxy, error) && !IsReodeProxy(proxy) &&
      !fs::exists(backup, error)) {
    fs::rename(proxy, backup, error);
    if (error) {
      return std::unexpected(std::format("cannot back up {}: {}",
                                         Narrow(proxy), error.message()));
    }
  }
  // Reode.dll first: the proxy is what makes the game load it.
  if (auto result = WriteFileAtomically(game_dir / kReodeDllName,
                                        payload.reode_dll);
      !result) {
    return result;
  }
  if (!payload.geode_dll.empty()) {
    if (auto result = WriteFileAtomically(
            game_dir / L"reode" / L"compat" / L"Geode.dll", payload.geode_dll);
        !result) {
      return result;
    }
  }
  for (const Payload::Mod& mod : payload.mods) {
    if (auto result = WriteFileAtomically(
            game_dir / L"reode" / L"mods" / mod.file_name, mod.data);
        !result) {
      return result;
    }
  }
  return WriteFileAtomically(proxy, payload.proxy_dll);
}

std::expected<void, std::string> Uninstall(const fs::path& game_dir,
                                           bool remove_data) {
  const fs::path proxy = game_dir / kProxyName;
  const fs::path backup = game_dir / kProxyBackupName;
  std::error_code error;
  // The proxy first: without it the game no longer loads Reode at all.
  if (IsReodeProxy(proxy)) fs::remove(proxy, error);
  if (!error && fs::exists(backup, error)) fs::rename(backup, proxy, error);
  if (!error) fs::remove(game_dir / kReodeDllName, error);
  if (!error) fs::remove_all(game_dir / L"reode" / L"compat", error);
  if (!error && remove_data && fs::exists(game_dir / L"reode", error)) {
    // Save backups are the player's progress: they always stay.
    for (const auto& entry :
         fs::directory_iterator(game_dir / L"reode", error)) {
      if (entry.path().filename() == L"backups") continue;
      fs::remove_all(entry.path(), error);
      if (error) break;
    }
  }
  if (error) {
    return std::unexpected("could not remove every file: " + error.message());
  }
  return {};
}

}  // namespace reode::setup
