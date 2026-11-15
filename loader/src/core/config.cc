// Copyright 2026 The Reode Authors

#include "core/config.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

#include "nlohmann/json.hpp"

namespace reode {
namespace {

using Json = nlohmann::json;

std::string_view SeverityToString(LogSeverity severity) {
  switch (severity) {
    case LogSeverity::kDebug:
      return "debug";
    case LogSeverity::kInfo:
      return "info";
    case LogSeverity::kWarning:
      return "warning";
    case LogSeverity::kError:
      return "error";
  }
  return "debug";
}

LogSeverity SeverityFromString(std::string_view text) {
  if (text == "info") return LogSeverity::kInfo;
  if (text == "warning") return LogSeverity::kWarning;
  if (text == "error") return LogSeverity::kError;
  return LogSeverity::kDebug;
}

}  // namespace

Config LoadConfig(const std::filesystem::path& file) {
  Config config;
  std::ifstream stream(file, std::ios::binary);
  if (!stream) return config;
  std::string text((std::istreambuf_iterator<char>(stream)),
                   std::istreambuf_iterator<char>());
  Json json = Json::parse(text, nullptr, /*allow_exceptions=*/false);
  if (json.is_discarded() || !json.is_object()) {
    LogWarning("config.json is not valid JSON, using defaults");
    return config;
  }
  if (auto it = json.find("show_console");
      it != json.end() && it->is_boolean()) {
    config.show_console = it->get<bool>();
  }
  if (auto it = json.find("log_level"); it != json.end() && it->is_string()) {
    config.log_level = SeverityFromString(it->get<std::string>());
  }
  if (auto it = json.find("game_version");
      it != json.end() && it->is_string()) {
    config.game_version = it->get<std::string>();
  }
  if (auto it = json.find("load_geode_mods");
      it != json.end() && it->is_boolean()) {
    config.load_geode_mods = it->get<bool>();
  }
  if (auto it = json.find("disabled_mods");
      it != json.end() && it->is_array()) {
    for (const Json& id : *it) {
      if (id.is_string()) config.disabled_mods.insert(id.get<std::string>());
    }
  }
  if (auto it = json.find("save_backups");
      it != json.end() && it->is_number_integer()) {
    config.save_backups = it->get<int>();
  }
  if (auto it = json.find("safe_mode_after_crash");
      it != json.end() && it->is_boolean()) {
    config.safe_mode_after_crash = it->get<bool>();
  }
  return config;
}

void SaveConfig(const std::filesystem::path& file, const Config& config) {
  Json json = {
      {"show_console", config.show_console},
      {"log_level", std::string(SeverityToString(config.log_level))},
      {"game_version", config.game_version},
      {"load_geode_mods", config.load_geode_mods},
      {"disabled_mods", config.disabled_mods},
      {"save_backups", config.save_backups},
      {"safe_mode_after_crash", config.safe_mode_after_crash},
  };
  std::ofstream stream(file, std::ios::binary | std::ios::trunc);
  stream << json.dump(2) << '\n';
}

}  // namespace reode
