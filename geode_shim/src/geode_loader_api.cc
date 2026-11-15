// Copyright 2026 The Reode Authors
//
// The rest of geode::Loader and geode::dirs, mapped onto Reode's layout:
// Geode's "geode/" directory is Reode's "reode/" directory.

#include <shlobj.h>
#include <windows.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "Geode/loader/Dirs.hpp"
#include "Geode/loader/Loader.hpp"
#include "Geode/loader/Mod.hpp"
#include "host.h"
#include "main_thread.h"
#include "mod_registry.h"
#include "reode/geode_shim_host.h"

namespace reode::geode_shim {
namespace {

std::filesystem::path Ensure(std::filesystem::path path) {
  std::error_code error;
  std::filesystem::create_directories(path, error);
  return path;
}

std::filesystem::path ExecutableDirectory() {
  std::wstring path(MAX_PATH, L'\0');
  DWORD length =
      GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
  path.resize(length);
  return std::filesystem::path(path).parent_path();
}

std::filesystem::path LocalAppData() {
  PWSTR raw = nullptr;
  std::filesystem::path result;
  if (SUCCEEDED(
          SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &raw))) {
    result = raw;
  }
  CoTaskMemFree(raw);
  return result;
}

}  // namespace
}  // namespace reode::geode_shim

namespace geode {

using reode::geode_shim::Ensure;
using reode::geode_shim::ModRegistry;

// --- Loader -----------------------------------------------------------------

VersionInfo Loader::getVersion() {
  return VersionInfo::parse(REODE_GEODE_SHIM_SDK_VERSION)
      .unwrapOr(VersionInfo());
}

bool Loader::isModInstalled(std::string_view id) const {
  return ModRegistry::Instance().FindById(id) != nullptr;
}

Mod* Loader::getInstalledMod(std::string_view id) const {
  return ModRegistry::Instance().FindById(id);
}

bool Loader::isModLoaded(std::string_view id) const {
  Mod* mod = ModRegistry::Instance().FindById(id);
  return mod != nullptr && mod->isLoaded();
}

Mod* Loader::getLoadedMod(std::string_view id) const {
  Mod* mod = ModRegistry::Instance().FindById(id);
  return mod != nullptr && mod->isLoaded() ? mod : nullptr;
}

std::vector<Mod*> Loader::getAllMods() { return ModRegistry::Instance().All(); }

std::vector<std::string> Loader::getLaunchArgumentNames() const { return {}; }

bool Loader::hasLaunchArgument(std::string_view /*name*/) const {
  return false;
}

std::optional<std::string> Loader::getLaunchArgument(
    std::string_view /*name*/) const {
  return std::nullopt;
}

bool Loader::getLaunchFlag(std::string_view /*name*/) const { return false; }

void Loader::queueInMainThread(ScheduledFunction&& func) {
  reode::geode_shim::QueueInMainThread(std::move(func));
}

std::string Loader::getGameVersion() {
  return reode::geode_shim::HasApi() ? reode::geode_shim::Api().game_version()
                                     : std::string("unknown");
}

bool Loader::isPatchless() const { return false; }

// --- dirs -------------------------------------------------------------------

std::filesystem::path dirs::getGameDir() {
  return reode::geode_shim::ExecutableDirectory();
}

std::filesystem::path dirs::getSaveDir() {
  return Ensure(reode::geode_shim::LocalAppData() / L"GeometryDash");
}

std::filesystem::path dirs::getGeodeDir() {
  return Ensure(getGameDir() / L"reode");
}

std::filesystem::path dirs::getGeodeSaveDir() { return getGeodeDir(); }

std::filesystem::path dirs::getGeodeResourcesDir() {
  return Ensure(getGeodeDir() / L"resources");
}

std::filesystem::path dirs::getGeodeLogDir() {
  return Ensure(getGeodeDir() / L"logs");
}

std::filesystem::path dirs::getTempDir() {
  return Ensure(getGeodeDir() / L"temp");
}

std::filesystem::path dirs::getModsDir() {
  return Ensure(getGeodeDir() / L"mods");
}

std::filesystem::path dirs::getModsSaveDir() {
  return Ensure(getGeodeDir() / L"saves");
}

std::filesystem::path dirs::getModRuntimeDir() {
  return Ensure(getGeodeDir() / L"unzipped");
}

std::filesystem::path dirs::getModConfigDir() {
  return Ensure(getGeodeDir() / L"config");
}

std::filesystem::path dirs::getIndexDir() {
  return Ensure(getGeodeDir() / L"index");
}

std::filesystem::path dirs::getCrashlogsDir() {
  return Ensure(getGeodeDir() / L"crashlogs");
}

std::filesystem::path dirs::getModPersistentDir() {
  return Ensure(getSaveDir() / L"reode-persistent");
}

std::filesystem::path dirs::getResourcesDir() {
  return getGameDir() / L"Resources";
}

}  // namespace geode
