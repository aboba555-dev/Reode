// Copyright 2026 The Reode Authors

#include "mods/mod_metadata.h"

#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <utility>

#include "nlohmann/json.hpp"

namespace reode::mods {
namespace {

using Json = nlohmann::json;

constexpr std::size_t kMaxModIdLength = 64;

std::string GetString(const Json& object, const char* key) {
  auto it = object.find(key);
  if (it == object.end() || !it->is_string()) return {};
  return it->get<std::string>();
}

// "gd" is either a string or an object keyed by platform.
std::string ParseGameVersion(const Json& object) {
  auto it = object.find("gd");
  if (it == object.end()) return {};
  if (it->is_string()) return it->get<std::string>();
  if (it->is_object()) return GetString(*it, "win");
  return {};
}

std::expected<Dependency, std::string> MakeDependency(std::string id,
                                                      std::string_view version,
                                                      bool required) {
  if (!IsValidModId(id)) {
    return std::unexpected(std::format("invalid dependency id \"{}\"", id));
  }
  auto requirement = VersionRequirement::Parse(version);
  if (!requirement) {
    return std::unexpected(std::format(
        "invalid version requirement \"{}\" for dependency {}", version, id));
  }
  return Dependency{std::move(id), *requirement, required};
}

bool IsRequiredImportance(const Json& entry) {
  if (auto it = entry.find("required"); it != entry.end() && it->is_boolean()) {
    return it->get<bool>();
  }
  std::string importance = GetString(entry, "importance");
  return importance.empty() || importance == "required";
}

// Accepts the array form [{"id", "version", "importance"}] and the object
// form {"id": "version"} / {"id": {"version", "importance"}}.
std::expected<std::vector<Dependency>, std::string> ParseDependencies(
    const Json& object) {
  std::vector<Dependency> result;
  auto it = object.find("dependencies");
  if (it == object.end() || it->is_null()) return result;

  if (it->is_array()) {
    for (const Json& entry : *it) {
      if (!entry.is_object()) {
        return std::unexpected("dependency entries must be objects");
      }
      auto dependency =
          MakeDependency(GetString(entry, "id"), GetString(entry, "version"),
                         IsRequiredImportance(entry));
      if (!dependency) return std::unexpected(dependency.error());
      result.push_back(*std::move(dependency));
    }
    return result;
  }
  if (it->is_object()) {
    for (const auto& [id, value] : it->items()) {
      std::expected<Dependency, std::string> dependency;
      if (value.is_string()) {
        dependency = MakeDependency(id, value.get<std::string>(), true);
      } else if (value.is_object()) {
        dependency = MakeDependency(id, GetString(value, "version"),
                                    IsRequiredImportance(value));
      } else {
        return std::unexpected(std::format("invalid dependency {}", id));
      }
      if (!dependency) return std::unexpected(dependency.error());
      result.push_back(*std::move(dependency));
    }
    return result;
  }
  return std::unexpected("\"dependencies\" must be an array or an object");
}

}  // namespace

bool IsValidModId(std::string_view id) {
  if (id.empty() || id.size() > kMaxModIdLength) return false;
  if (id.front() == '.' || id.back() == '.') return false;
  if (id.find("..") != std::string_view::npos) return false;
  for (char c : id) {
    bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' ||
              c == '_' || c == '-';
    if (!ok) return false;
  }
  return true;
}

std::expected<ModMetadata, std::string> ParseModMetadata(
    std::string_view json_text) {
  Json json = Json::parse(json_text, nullptr, /*allow_exceptions=*/false);
  if (json.is_discarded()) return std::unexpected("mod.json is not valid JSON");
  if (!json.is_object()) return std::unexpected("mod.json must be an object");

  ModMetadata metadata;
  if (json.contains("reode")) {
    metadata.format = ModFormat::kReode;
    const Json& value = json["reode"];
    metadata.loader_version =
        value.is_string() ? value.get<std::string>()
                          : (value.is_number() ? value.dump() : std::string());
  } else if (json.contains("geode")) {
    metadata.format = ModFormat::kGeode;
    metadata.loader_version = GetString(json, "geode");
  } else {
    return std::unexpected(
        "mod.json has neither a \"reode\" nor a \"geode\" key");
  }

  metadata.id = GetString(json, "id");
  if (!IsValidModId(metadata.id)) {
    return std::unexpected(std::format("invalid mod id \"{}\"", metadata.id));
  }
  metadata.name = GetString(json, "name");
  if (metadata.name.empty()) metadata.name = metadata.id;

  std::string version_text = GetString(json, "version");
  auto version = Version::Parse(version_text);
  if (!version) {
    return std::unexpected(std::format("invalid version \"{}\"", version_text));
  }
  metadata.version = *std::move(version);

  if (auto it = json.find("developers"); it != json.end() && it->is_array()) {
    for (const Json& developer : *it) {
      if (developer.is_string()) {
        metadata.developers.push_back(developer.get<std::string>());
      }
    }
  }
  if (std::string developer = GetString(json, "developer");
      !developer.empty()) {
    metadata.developers.push_back(std::move(developer));
  }

  metadata.description = GetString(json, "description");
  metadata.game_version = ParseGameVersion(json);

  auto dependencies = ParseDependencies(json);
  if (!dependencies) return std::unexpected(dependencies.error());
  metadata.dependencies = *std::move(dependencies);

  if (auto resources = json.find("resources");
      resources != json.end() && resources->is_object()) {
    if (auto sheets = resources->find("spritesheets");
        sheets != resources->end() && sheets->is_object()) {
      for (const auto& [name, files] : sheets->items()) {
        bool safe =
            !name.empty() && name.find_first_of("/\\:.") == std::string::npos;
        if (safe) metadata.spritesheets.push_back(name);
      }
    }
  }

  metadata.binary = GetString(json, "binary");
  if (metadata.binary.empty()) metadata.binary = metadata.id + ".dll";
  if (metadata.binary.find_first_of("/\\:") != std::string::npos ||
      metadata.binary.find("..") != std::string::npos) {
    return std::unexpected("\"binary\" must be a plain file name");
  }
  return metadata;
}

}  // namespace reode::mods
