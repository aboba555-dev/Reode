// Copyright 2026 The Reode Authors
//
// geode::Mod and the core of geode::Loader, implemented on top of Reode's
// ModRegistry.

#include <algorithm>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "Geode/loader/Dirs.hpp"
#include "Geode/loader/Hook.hpp"
#include "Geode/loader/Loader.hpp"
#include "Geode/loader/Mod.hpp"
#include "Geode/loader/ModMetadata.hpp"
#include "Geode/loader/SettingV3.hpp"
#include "Geode/utils/file.hpp"
#include "geode_impl.h"
#include "host.h"
#include "mod_registry.h"

namespace geode {
namespace {

constexpr char kSavedDataFile[] = "saved.json";
constexpr char kSettingsFile[] = "settings.json";

std::filesystem::path Ensure(std::filesystem::path path) {
  std::error_code error;
  std::filesystem::create_directories(path, error);
  return path;
}

}  // namespace

class Loader::Impl {};

// --- Construction and identity ----------------------------------------------

Mod::Mod(ModMetadata const& metadata)
    : m_impl(std::make_unique<Impl>(
          reode::geode_shim::ModRegistry::Instance().TakeConstructionState())) {
  m_impl->metadata = metadata;
  reode::geode_shim::ModRegistry::Instance().Register(this, m_impl.get());
}

Mod::~Mod() = default;

ZStringView Mod::getID() const { return m_impl->id; }

ZStringView Mod::getName() const { return m_impl->name; }

std::vector<std::string> Mod::getDevelopers() const {
  return m_impl->developers;
}

std::optional<std::string> Mod::getDescription() const {
  return m_impl->metadata.getDescription();
}

std::optional<std::string> Mod::getDetails() const {
  return m_impl->metadata.getDetails();
}

VersionInfo Mod::getVersion() const { return m_impl->metadata.getVersion(); }

ModMetadata const& Mod::getMetadata() const { return m_impl->metadata; }

void Mod::setMetadata(ModMetadata const& metadata) {
  m_impl->metadata = metadata;
}

bool Mod::isLoaded() const { return m_impl->loaded; }

bool Mod::isCurrentlyLoading() const {
  return reode::geode_shim::ModRegistry::Instance().current() == this;
}

bool Mod::isOrWillBeEnabled() const { return true; }

// Only Geode's own built-in mod is "internal"; every mod Reode loads is a
// regular external mod.
bool Mod::isInternal() const { return false; }

bool Mod::needsEarlyLoad() const { return m_impl->metadata.needsEarlyLoad(); }

bool Mod::shouldLoad() const { return true; }

bool Mod::isUninstalled() const { return false; }

bool Mod::hasUnresolvedDependencies() const { return false; }

bool Mod::hasUnresolvedIncompatibilities() const { return false; }

bool Mod::depends(std::string_view id) const {
  return std::ranges::any_of(
      m_impl->metadata.getDependencies(),
      [&](const ModMetadata::Dependency& dep) { return dep.getID() == id; });
}

std::vector<Mod*> Mod::getDependants() const {
  std::vector<Mod*> result;
  for (Mod* mod : reode::geode_shim::ModRegistry::Instance().All()) {
    if (mod != this && mod->depends(m_impl->id)) result.push_back(mod);
  }
  return result;
}

std::string Mod::expandSpriteName(std::string_view name) {
  return std::format("{}/{}", m_impl->id, name);
}

// --- Directories ------------------------------------------------------------

std::filesystem::path Mod::getResourcesDir() const {
  return m_impl->resources_dir / "resources" / m_impl->id;
}

std::filesystem::path Mod::getSaveDir() const {
  return Ensure(m_impl->save_dir);
}

std::filesystem::path Mod::getConfigDir(bool create) const {
  std::filesystem::path dir = dirs::getModConfigDir() / m_impl->id;
  return create ? Ensure(dir) : dir;
}

std::filesystem::path Mod::getPersistentDir(bool create) const {
  std::filesystem::path dir = dirs::getModPersistentDir() / m_impl->id;
  return create ? Ensure(dir) : dir;
}

std::filesystem::path Mod::getTempDir() const { return m_impl->resources_dir; }

// --- Save data and settings -------------------------------------------------

matjson::Value& Mod::getSaveContainer() { return m_impl->save_container; }

matjson::Value const& Mod::getSaveContainerConst() const {
  return m_impl->save_container;
}

matjson::Value& Mod::getSaveContainerTemp() { return m_impl->save_container; }

matjson::Value& Mod::getSavedSettingsData() { return m_impl->saved_settings; }

bool Mod::hasSavedValue(std::string_view key) {
  return m_impl->save_container.contains(key);
}

Result<> Mod::loadData() {
  std::filesystem::path dir = getSaveDir();
  if (auto saved = utils::file::readJson(dir / kSavedDataFile);
      saved && saved.unwrap().isObject()) {
    m_impl->save_container = saved.unwrap();
  }
  if (auto settings = utils::file::readJson(dir / kSettingsFile);
      settings && settings.unwrap().isObject()) {
    m_impl->saved_settings = settings.unwrap();
  }
  for (const auto& [key, setting] : m_impl->settings) {
    if (m_impl->saved_settings.contains(key)) {
      setting->load(m_impl->saved_settings[key]);
    }
  }
  return Ok();
}

Result<> Mod::saveData() {
  for (const auto& [key, setting] : m_impl->settings) {
    matjson::Value value;
    if (setting->save(value)) m_impl->saved_settings[key] = std::move(value);
  }
  std::filesystem::path dir = getSaveDir();
  GEODE_UNWRAP(utils::file::writeStringSafe(dir / kSavedDataFile,
                                            m_impl->save_container.dump()));
  GEODE_UNWRAP(utils::file::writeStringSafe(dir / kSettingsFile,
                                            m_impl->saved_settings.dump()));
  return Ok();
}

bool Mod::hasSettings() const { return !m_impl->settings.empty(); }

std::vector<std::string> Mod::getSettingKeys() const {
  std::vector<std::string> keys;
  for (const auto& [key, setting] : m_impl->settings) keys.push_back(key);
  return keys;
}

bool Mod::hasSetting(std::string_view key) const {
  return getSetting(key) != nullptr;
}

std::shared_ptr<SettingV3> Mod::getSetting(std::string_view key) const {
  for (const auto& [setting_key, setting] : m_impl->settings) {
    if (setting_key == key) return setting;
  }
  return nullptr;
}

Result<> Mod::registerCustomSettingType(std::string_view type,
                                        SettingGenerator generator) {
  // Custom settings are created from mod.json once the type is known.
  std::string key(type);
  if (m_impl->custom_settings.contains(key)) {
    return Err(std::format("setting type \"{}\" is already registered", key));
  }
  for (const auto& [setting_key, json] : m_impl->metadata.getSettings()) {
    std::string setting_type = json["type"].asString().unwrapOr("");
    if (setting_type != "custom:" + key && setting_type != key) continue;
    auto created = generator(setting_key, m_impl->id, json);
    if (!created) {
      reode::geode_shim::Log(m_impl->reode_mod, kReodeLogWarning,
                             std::format("custom setting {}: {}", setting_key,
                                         created.unwrapErr()));
      continue;
    }
    std::shared_ptr<SettingV3> setting = std::move(created).unwrap();
    if (m_impl->saved_settings.contains(setting_key)) {
      setting->load(m_impl->saved_settings[setting_key]);
    }
    m_impl->settings.emplace_back(setting_key, std::move(setting));
  }
  m_impl->custom_settings.emplace(std::move(key), std::move(generator));
  return Ok();
}

// --- Launch arguments (Reode has none) --------------------------------------

std::vector<std::string> Mod::getLaunchArgumentNames() const { return {}; }

bool Mod::hasLaunchArgument(std::string_view /*name*/) const { return false; }

std::optional<std::string> Mod::getLaunchArgument(
    std::string_view /*name*/) const {
  return std::nullopt;
}

bool Mod::getLaunchFlag(std::string_view /*name*/) const { return false; }

// --- Hooks and logging ------------------------------------------------------

Result<Hook*> Mod::claimHook(std::shared_ptr<Hook> hook) {
  if (hook == nullptr) return Err("hook is null");
  Hook* raw = hook.get();
  if (raw->getOwner() != nullptr && raw->getOwner() != this) {
    return Err(std::format("hook {} is already owned by another mod",
                           raw->getDisplayName()));
  }
  hook->m_impl->owner = this;
  if (m_impl->loaded && hook->getAutoEnable()) {
    if (auto result = hook->enable(); !result) {
      return Err(result.unwrapErr());
    }
  }
  m_impl->hooks.push_back(std::move(hook));
  return Ok(raw);
}

Result<> Mod::disownHook(Hook* hook) {
  auto it = std::ranges::find_if(
      m_impl->hooks, [&](const auto& owned) { return owned.get() == hook; });
  if (it == m_impl->hooks.end()) return Err("hook is not owned by this mod");
  GEODE_UNWRAP(hook->disable());
  m_impl->hooks.erase(it);
  return Ok();
}

std::vector<Hook*> Mod::getHooks() const {
  std::vector<Hook*> hooks;
  for (const auto& hook : m_impl->hooks) hooks.push_back(hook.get());
  return hooks;
}

bool Mod::isLoggingEnabled() const { return m_impl->logging_enabled; }

void Mod::setLoggingEnabled(bool enabled) { m_impl->logging_enabled = enabled; }

Severity Mod::getLogLevel() const {
  return static_cast<Severity::type>(m_impl->log_level);
}

void Mod::setLogLevel(Severity level) { m_impl->log_level = level; }

// --- Loader core ------------------------------------------------------------

Loader::Loader() : m_impl(std::make_unique<Impl>()) {}

Loader::~Loader() = default;

Loader* Loader::get() {
  static Loader* const kLoader = new Loader();
  return kLoader;
}

Mod* Loader::takeNextMod() {
  return reode::geode_shim::ModRegistry::Instance().current();
}

}  // namespace geode
