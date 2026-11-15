// Copyright 2026 The Reode Authors
//
// geode::ModMetadata and its helper classes. Reode fills them from mod.json
// when a Geode mod is loaded; mods only read them.

#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "fmt/format.h"
#include "matjson.hpp"

namespace geode {

// --- ModMetadataLinks -------------------------------------------------------

class ModMetadataLinks::Impl {
 public:
  std::optional<std::string> homepage;
  std::optional<std::string> source;
  std::optional<std::string> community;
};

ModMetadataLinks::ModMetadataLinks() : m_impl(std::make_unique<Impl>()) {}
ModMetadataLinks::ModMetadataLinks(ModMetadataLinks const& other)
    : m_impl(std::make_unique<Impl>(*other.m_impl)) {}
ModMetadataLinks::ModMetadataLinks(ModMetadataLinks&& other) noexcept
    : m_impl(std::move(other.m_impl)) {}
ModMetadataLinks& ModMetadataLinks::operator=(ModMetadataLinks const& other) {
  m_impl = std::make_unique<Impl>(*other.m_impl);
  return *this;
}
ModMetadataLinks& ModMetadataLinks::operator=(
    ModMetadataLinks&& other) noexcept {
  m_impl = std::move(other.m_impl);
  return *this;
}
ModMetadataLinks::~ModMetadataLinks() = default;

std::optional<std::string> ModMetadataLinks::getHomepageURL() const {
  return m_impl->homepage;
}
std::optional<std::string> ModMetadataLinks::getSourceURL() const {
  return m_impl->source;
}
std::optional<std::string> ModMetadataLinks::getCommunityURL() const {
  return m_impl->community;
}
ModMetadataLinks::Impl* ModMetadataLinks::getImpl() { return m_impl.get(); }

// --- ModMetadata ------------------------------------------------------------

class ModMetadata::Impl {
 public:
  std::filesystem::path path;
  std::string binary_name;
  VersionInfo version;
  std::string id;
  std::string name;
  std::vector<std::string> developers;
  std::optional<std::string> description;
  std::optional<std::string> details;
  std::optional<std::string> changelog;
  std::optional<std::string> support_info;
  std::optional<std::string> repository;
  ModMetadataLinks links;
  std::optional<IssuesInfo> issues;
  std::vector<Dependency> dependencies;
  std::vector<Incompatibility> incompatibilities;
  std::vector<std::string> spritesheets;
  std::vector<std::pair<std::string, matjson::Value>> settings;
  std::unordered_set<std::string> tags;
  bool needs_early_load = false;
  bool is_api = false;
  std::optional<std::string> game_version;
  VersionInfo geode_version;
  std::vector<std::string> errors;
};

ModMetadata::ModMetadata() : m_impl(std::make_unique<Impl>()) {}
ModMetadata::ModMetadata(std::string id) : ModMetadata() {
  m_impl->id = std::move(id);
}
ModMetadata::ModMetadata(ModMetadata const& other)
    : m_impl(std::make_unique<Impl>(*other.m_impl)) {}
ModMetadata::ModMetadata(ModMetadata&& other) noexcept
    : m_impl(std::move(other.m_impl)) {}
ModMetadata& ModMetadata::operator=(ModMetadata const& other) {
  m_impl = std::make_unique<Impl>(*other.m_impl);
  return *this;
}
ModMetadata& ModMetadata::operator=(ModMetadata&& other) noexcept {
  m_impl = std::move(other.m_impl);
  return *this;
}
ModMetadata::~ModMetadata() = default;

std::filesystem::path const& ModMetadata::getPath() const {
  return m_impl->path;
}
ZStringView ModMetadata::getBinaryName() const { return m_impl->binary_name; }
VersionInfo ModMetadata::getVersion() const { return m_impl->version; }
ZStringView ModMetadata::getID() const { return m_impl->id; }
ZStringView ModMetadata::getName() const { return m_impl->name; }
std::vector<std::string> const& ModMetadata::getDevelopers() const {
  return m_impl->developers;
}
std::optional<std::string> const& ModMetadata::getDescription() const {
  return m_impl->description;
}
std::optional<std::string> const& ModMetadata::getDetails() const {
  return m_impl->details;
}
std::optional<std::string> const& ModMetadata::getChangelog() const {
  return m_impl->changelog;
}
std::optional<std::string> const& ModMetadata::getSupportInfo() const {
  return m_impl->support_info;
}
ModMetadataLinks const& ModMetadata::getLinks() const { return m_impl->links; }
ModMetadataLinks& ModMetadata::getLinksMut() { return m_impl->links; }
std::optional<ModMetadata::IssuesInfo> const& ModMetadata::getIssues() const {
  return m_impl->issues;
}
std::vector<ModMetadata::Dependency> const& ModMetadata::getDependencies()
    const {
  return m_impl->dependencies;
}
std::vector<ModMetadata::Incompatibility> const&
ModMetadata::getIncompatibilities() const {
  return m_impl->incompatibilities;
}
std::vector<std::string> const& ModMetadata::getSpritesheets() const {
  return m_impl->spritesheets;
}
std::vector<std::pair<std::string, matjson::Value>> const&
ModMetadata::getSettings() const {
  return m_impl->settings;
}
std::unordered_set<std::string> const& ModMetadata::getTags() const {
  return m_impl->tags;
}
bool ModMetadata::needsEarlyLoad() const { return m_impl->needs_early_load; }
bool ModMetadata::isAPI() const { return m_impl->is_api; }
std::optional<std::string> ModMetadata::getGameVersion() const {
  return m_impl->game_version;
}
VersionInfo ModMetadata::getGeodeVersion() const {
  return m_impl->geode_version;
}
int ModMetadata::getLoadPriority() const { return 0; }
bool ModMetadata::hasErrors() const { return !m_impl->errors.empty(); }
std::vector<std::string> const& ModMetadata::getErrors() const {
  return m_impl->errors;
}

void ModMetadata::setPath(std::filesystem::path value) {
  m_impl->path = std::move(value);
}
void ModMetadata::setBinaryName(std::string value) {
  m_impl->binary_name = std::move(value);
}
void ModMetadata::setVersion(VersionInfo value) { m_impl->version = value; }
void ModMetadata::setID(std::string value) { m_impl->id = std::move(value); }
void ModMetadata::setName(std::string value) {
  m_impl->name = std::move(value);
}
void ModMetadata::setDeveloper(std::string value) {
  m_impl->developers = {std::move(value)};
}
void ModMetadata::setDevelopers(std::vector<std::string> value) {
  m_impl->developers = std::move(value);
}
void ModMetadata::setDescription(std::optional<std::string> value) {
  m_impl->description = std::move(value);
}
void ModMetadata::setDetails(std::optional<std::string> value) {
  m_impl->details = std::move(value);
}
void ModMetadata::setChangelog(std::optional<std::string> value) {
  m_impl->changelog = std::move(value);
}
void ModMetadata::setSupportInfo(std::optional<std::string> value) {
  m_impl->support_info = std::move(value);
}
void ModMetadata::setRepository(std::optional<std::string> value) {
  m_impl->repository = std::move(value);
}
void ModMetadata::setSpritesheets(std::vector<std::string> value) {
  m_impl->spritesheets = std::move(value);
}
void ModMetadata::setSettings(
    std::vector<std::pair<std::string, matjson::Value>> value) {
  m_impl->settings = std::move(value);
}
void ModMetadata::setTags(std::unordered_set<std::string> value) {
  m_impl->tags = std::move(value);
}
void ModMetadata::setNeedsEarlyLoad(bool value) {
  m_impl->needs_early_load = value;
}
void ModMetadata::setIsAPI(bool value) { m_impl->is_api = value; }
void ModMetadata::setGameVersion(std::string value) {
  m_impl->game_version = std::move(value);
}
void ModMetadata::setGeodeVersion(VersionInfo value) {
  m_impl->geode_version = value;
}

std::string ModMetadata::formatDeveloperDisplayString(
    std::vector<std::string> const& developers) {
  switch (developers.size()) {
    case 0:
      return "Unknown";
    case 1:
      return developers.front();
    case 2:
      return developers[0] + " & " + developers[1];
    default:
      return fmt::format("{} & {} more", developers.front(),
                         developers.size() - 1);
  }
}

bool ModMetadata::validateID(std::string_view id) {
  if (id.empty()) return false;
  for (char c : id) {
    bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' ||
              c == '_' || c == '-';
    if (!ok) return false;
  }
  return true;
}

// --- Dependency / Incompatibility / IssuesInfo ------------------------------

class ModMetadata::Dependency::Impl {
 public:
  std::string id;
  ComparableVersionInfo version;
  bool required = true;
  Mod* mod = nullptr;
  matjson::Value settings;
};

ModMetadata::Dependency::Dependency() : m_impl(std::make_unique<Impl>()) {}
ModMetadata::Dependency::Dependency(Dependency const& other)
    : m_impl(std::make_unique<Impl>(*other.m_impl)) {}
ModMetadata::Dependency::Dependency(Dependency&& other) noexcept
    : m_impl(std::move(other.m_impl)) {}
ModMetadata::Dependency& ModMetadata::Dependency::operator=(
    Dependency const& other) {
  m_impl = std::make_unique<Impl>(*other.m_impl);
  return *this;
}
ModMetadata::Dependency& ModMetadata::Dependency::operator=(
    Dependency&& other) noexcept {
  m_impl = std::move(other.m_impl);
  return *this;
}
ModMetadata::Dependency::~Dependency() = default;
std::string const& ModMetadata::Dependency::getID() const { return m_impl->id; }
void ModMetadata::Dependency::setID(std::string value) {
  m_impl->id = std::move(value);
}
ComparableVersionInfo const& ModMetadata::Dependency::getVersion() const {
  return m_impl->version;
}
void ModMetadata::Dependency::setVersion(ComparableVersionInfo value) {
  m_impl->version = value;
}
bool ModMetadata::Dependency::isRequired() const { return m_impl->required; }
void ModMetadata::Dependency::setRequired(bool value) {
  m_impl->required = value;
}
Mod* ModMetadata::Dependency::getMod() const { return m_impl->mod; }
void ModMetadata::Dependency::setMod(Mod* mod) { m_impl->mod = mod; }
matjson::Value const& ModMetadata::Dependency::getSettings() const {
  return m_impl->settings;
}
void ModMetadata::Dependency::setSettings(matjson::Value value) {
  m_impl->settings = std::move(value);
}

class ModMetadata::Incompatibility::Impl {
 public:
  std::string id;
  ComparableVersionInfo version;
  bool breaking = false;
  Mod* mod = nullptr;
};

ModMetadata::Incompatibility::Incompatibility()
    : m_impl(std::make_unique<Impl>()) {}
ModMetadata::Incompatibility::Incompatibility(Incompatibility const& other)
    : m_impl(std::make_unique<Impl>(*other.m_impl)) {}
ModMetadata::Incompatibility::Incompatibility(Incompatibility&& other) noexcept
    : m_impl(std::move(other.m_impl)) {}
ModMetadata::Incompatibility& ModMetadata::Incompatibility::operator=(
    Incompatibility const& other) {
  m_impl = std::make_unique<Impl>(*other.m_impl);
  return *this;
}
ModMetadata::Incompatibility& ModMetadata::Incompatibility::operator=(
    Incompatibility&& other) noexcept {
  m_impl = std::move(other.m_impl);
  return *this;
}
ModMetadata::Incompatibility::~Incompatibility() = default;
std::string const& ModMetadata::Incompatibility::getID() const {
  return m_impl->id;
}
void ModMetadata::Incompatibility::setID(std::string value) {
  m_impl->id = std::move(value);
}
ComparableVersionInfo const& ModMetadata::Incompatibility::getVersion() const {
  return m_impl->version;
}
void ModMetadata::Incompatibility::setVersion(ComparableVersionInfo value) {
  m_impl->version = value;
}
bool ModMetadata::Incompatibility::isBreaking() const {
  return m_impl->breaking;
}
void ModMetadata::Incompatibility::setBreaking(bool value) {
  m_impl->breaking = value;
}
Mod* ModMetadata::Incompatibility::getMod() const { return m_impl->mod; }
void ModMetadata::Incompatibility::setMod(Mod* mod) { m_impl->mod = mod; }

class ModMetadata::IssuesInfo::Impl {
 public:
  std::string info;
  std::optional<std::string> url;
};

ModMetadata::IssuesInfo::IssuesInfo() : m_impl(std::make_unique<Impl>()) {}
ModMetadata::IssuesInfo::IssuesInfo(IssuesInfo const& other)
    : m_impl(std::make_unique<Impl>(*other.m_impl)) {}
ModMetadata::IssuesInfo::IssuesInfo(IssuesInfo&& other) noexcept
    : m_impl(std::move(other.m_impl)) {}
ModMetadata::IssuesInfo& ModMetadata::IssuesInfo::operator=(
    IssuesInfo const& other) {
  m_impl = std::make_unique<Impl>(*other.m_impl);
  return *this;
}
ModMetadata::IssuesInfo& ModMetadata::IssuesInfo::operator=(
    IssuesInfo&& other) noexcept {
  m_impl = std::move(other.m_impl);
  return *this;
}
ModMetadata::IssuesInfo::~IssuesInfo() = default;
std::string const& ModMetadata::IssuesInfo::getInfo() const {
  return m_impl->info;
}
void ModMetadata::IssuesInfo::setInfo(std::string value) {
  m_impl->info = std::move(value);
}
std::optional<std::string> const& ModMetadata::IssuesInfo::getURL() const {
  return m_impl->url;
}
void ModMetadata::IssuesInfo::setURL(std::optional<std::string> value) {
  m_impl->url = std::move(value);
}

}  // namespace geode
