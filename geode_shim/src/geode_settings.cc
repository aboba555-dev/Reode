// Copyright 2026 The Reode Authors
//
// Geode mod settings (SettingV3 and friends).
//
// TODO(reode): persist values in the mod's save directory and add a
// settings UI. Values currently start from their defaults every launch.

#include "geode_settings.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Geode/loader/Mod.hpp"
#include "Geode/loader/SettingV3.hpp"
#include "Geode/utils/string.hpp"
#include "fmt/format.h"
#include "host.h"
#include "matjson.hpp"
#include "mod_registry.h"

namespace reode::geode_shim {
namespace {

// Name and description for the setting being constructed. SettingV3's
// storage is private to SettingV3, so they are handed to SettingV3::init()
// through this slot instead of through the JSON validation helpers.
struct SettingSeed {
  std::optional<std::string> name;
  std::optional<std::string> description;
  bool requires_restart = false;
};
constinit SettingSeed* g_seed = nullptr;

std::optional<std::string> OptionalString(const matjson::Value& object,
                                          std::string_view key) {
  if (!object.contains(key)) return std::nullopt;
  auto value = object[key].asString();
  if (!value) return std::nullopt;
  return std::move(value).unwrap();
}

// Reads "default", which is either a value or {"win": value, ...}.
const matjson::Value& DefaultValue(const matjson::Value& json) {
  const matjson::Value& value = json["default"];
  if (value.isObject() && value.contains("win")) return value["win"];
  return value;
}

}  // namespace

std::vector<std::pair<std::string, std::shared_ptr<geode::SettingV3>>>
CreateSettings(const matjson::Value& mod_json, const std::string& mod_id,
               ReodeMod* reode_mod) {
  std::vector<std::pair<std::string, std::shared_ptr<geode::SettingV3>>>
      settings;
  if (!mod_json.contains("settings") || !mod_json["settings"].isObject()) {
    return settings;
  }
  for (const auto& [key, json] : mod_json["settings"]) {
    std::string type = json["type"].asString().unwrapOr("");
    SettingSeed seed{OptionalString(json, "name"),
                     OptionalString(json, "description"),
                     json["requires-restart"].asBool().unwrapOr(false)};
    g_seed = &seed;
    auto add = [&](auto parsed) {
      if (parsed) {
        settings.emplace_back(key, std::move(parsed).unwrap());
      } else {
        Log(reode_mod, kReodeLogWarning,
            std::format("setting {}: {}", key, parsed.unwrapErr()));
      }
    };
    if (type == "bool") {
      add(geode::BoolSettingV3::parse(key, mod_id, json));
    } else if (type == "int") {
      add(geode::IntSettingV3::parse(key, mod_id, json));
    } else if (type == "float") {
      add(geode::FloatSettingV3::parse(key, mod_id, json));
    } else if (type == "string") {
      add(geode::StringSettingV3::parse(key, mod_id, json));
    } else if (type == "file" || type == "folder" || type == "path") {
      add(geode::FileSettingV3::parse(key, mod_id, json));
    } else if (type == "color" || type == "rgb") {
      add(geode::Color3BSettingV3::parse(key, mod_id, json));
    } else if (type == "rgba") {
      add(geode::Color4BSettingV3::parse(key, mod_id, json));
    } else if (type.starts_with("custom:")) {
      // Created when the mod calls Mod::registerCustomSettingType.
      g_seed = nullptr;
      continue;
    } else {
      Log(reode_mod, kReodeLogWarning,
          std::format("setting {} has type \"{}\", which Reode does not "
                      "support yet",
                      key, type));
    }
    g_seed = nullptr;
  }
  return settings;
}

}  // namespace reode::geode_shim

namespace geode {

class SettingV3::GeodeImpl {
 public:
  std::string key;
  std::string mod_id;
  std::optional<std::string> name;
  std::optional<std::string> description;
  bool requires_restart = false;
};

SettingV3::SettingV3() : m_impl(std::make_shared<GeodeImpl>()) {}

SettingV3::~SettingV3() = default;

void SettingV3::init(std::string key, std::string modID) {
  m_impl->key = std::move(key);
  m_impl->mod_id = std::move(modID);
  if (reode::geode_shim::g_seed != nullptr) {
    m_impl->name = reode::geode_shim::g_seed->name;
    m_impl->description = reode::geode_shim::g_seed->description;
    m_impl->requires_restart = reode::geode_shim::g_seed->requires_restart;
  }
}

void SettingV3::markChanged() {
  SettingChangedEventV3(m_impl->mod_id, m_impl->key).send(shared_from_this());
}

std::string SettingV3::getKey() const { return m_impl->key; }

std::string SettingV3::getModID() const { return m_impl->mod_id; }

Mod* SettingV3::getMod() const {
  return reode::geode_shim::ModRegistry::Instance().FindById(m_impl->mod_id);
}

std::optional<std::string> SettingV3::getName() const { return m_impl->name; }

std::string SettingV3::getDisplayName() const {
  return m_impl->name.value_or(m_impl->key);
}

std::optional<std::string> SettingV3::getDescription() const {
  return m_impl->description;
}

std::optional<std::string> SettingV3::getEnableIf() const {
  return std::nullopt;
}

bool SettingV3::shouldEnable() const { return true; }

std::optional<std::string> SettingV3::getEnableIfDescription() const {
  return std::nullopt;
}

bool SettingV3::requiresRestart() const { return m_impl->requires_restart; }

// --- BoolSettingV3 ----------------------------------------------------------

class BoolSettingV3::Impl {};

BoolSettingV3::BoolSettingV3(PrivateMarker)
    : m_impl(std::make_shared<Impl>()) {}

Result<std::shared_ptr<BoolSettingV3>> BoolSettingV3::parse(
    std::string key, std::string modID, matjson::Value const& json) {
  auto value = reode::geode_shim::DefaultValue(json).asBool();
  if (!value) return Err("\"default\" must be a boolean");
  auto setting = std::make_shared<BoolSettingV3>(PrivateMarker());
  setting->init(std::move(key), std::move(modID));
  setting->setDefaultValue(value.unwrap());
  setting->reset();
  return Ok(std::move(setting));
}

Result<> BoolSettingV3::isValid(bool /*value*/) const { return Ok(); }

// TODO(reode): settings UI.
SettingNodeV3* BoolSettingV3::createNode(float /*width*/) { return nullptr; }

// --- IntSettingV3 -----------------------------------------------------------

class IntSettingV3::Impl {
 public:
  std::optional<int64_t> min;
  std::optional<int64_t> max;
};

IntSettingV3::IntSettingV3(PrivateMarker) : m_impl(std::make_shared<Impl>()) {}

Result<std::shared_ptr<IntSettingV3>> IntSettingV3::parse(
    std::string key, std::string modID, matjson::Value const& json) {
  auto value = reode::geode_shim::DefaultValue(json).asInt();
  if (!value) return Err("\"default\" must be an integer");
  auto setting = std::make_shared<IntSettingV3>(PrivateMarker());
  if (json.contains("min")) setting->m_impl->min = json["min"].asInt().ok();
  if (json.contains("max")) setting->m_impl->max = json["max"].asInt().ok();
  setting->init(std::move(key), std::move(modID));
  setting->setDefaultValue(value.unwrap());
  setting->reset();
  return Ok(std::move(setting));
}

Result<> IntSettingV3::isValid(int64_t value) const {
  if (m_impl->min && value < *m_impl->min) {
    return Err(std::format("value must be at least {}", *m_impl->min));
  }
  if (m_impl->max && value > *m_impl->max) {
    return Err(std::format("value must be at most {}", *m_impl->max));
  }
  return Ok();
}

std::optional<int64_t> IntSettingV3::getMinValue() const { return m_impl->min; }

std::optional<int64_t> IntSettingV3::getMaxValue() const { return m_impl->max; }

// TODO(reode): settings UI.
SettingNodeV3* IntSettingV3::createNode(float /*width*/) { return nullptr; }

// --- Remaining value settings -----------------------------------------------

namespace {

// Parses "default" (optionally per platform) as T.
template <typename T>
Result<T> ParseDefault(matjson::Value const& json) {
  auto value = reode::geode_shim::DefaultValue(json).as<T>();
  if (!value) {
    return Err(fmt::format("invalid \"default\": {}", value.unwrapErr()));
  }
  return Ok(value.unwrap());
}

}  // namespace

// The setting classes may only call their protected initializers from
// their own members, so every parse() finishes with this.
#define REODE_FINISH_SETTING(setting, key, mod_id, value) \
  (setting)->init(std::move(key), std::move(mod_id));     \
  (setting)->setDefaultValue((value).unwrap());           \
  (setting)->reset();                                     \
  return Ok(std::move(setting))

class FloatSettingV3::Impl {
 public:
  std::optional<double> min;
  std::optional<double> max;
};

FloatSettingV3::FloatSettingV3(PrivateMarker)
    : m_impl(std::make_shared<Impl>()) {}

Result<std::shared_ptr<FloatSettingV3>> FloatSettingV3::parse(
    std::string key, std::string modID, matjson::Value const& json) {
  auto setting = std::make_shared<FloatSettingV3>(PrivateMarker());
  if (json.contains("min")) setting->m_impl->min = json["min"].asDouble().ok();
  if (json.contains("max")) setting->m_impl->max = json["max"].asDouble().ok();
  auto value = ParseDefault<double>(json);
  if (!value) return Err(value.unwrapErr());
  REODE_FINISH_SETTING(setting, key, modID, value);
}

Result<> FloatSettingV3::isValid(double value) const {
  if (m_impl->min && value < *m_impl->min) {
    return Err(fmt::format("value must be at least {}", *m_impl->min));
  }
  if (m_impl->max && value > *m_impl->max) {
    return Err(fmt::format("value must be at most {}", *m_impl->max));
  }
  return Ok();
}

std::optional<double> FloatSettingV3::getMinValue() const {
  return m_impl->min;
}
std::optional<double> FloatSettingV3::getMaxValue() const {
  return m_impl->max;
}
SettingNodeV3* FloatSettingV3::createNode(float /*width*/) { return nullptr; }

class StringSettingV3::Impl {
 public:
  std::optional<std::vector<std::string>> options;
};

StringSettingV3::StringSettingV3(PrivateMarker)
    : m_impl(std::make_shared<Impl>()) {}

Result<std::shared_ptr<StringSettingV3>> StringSettingV3::parse(
    std::string key, std::string modID, matjson::Value const& json) {
  auto setting = std::make_shared<StringSettingV3>(PrivateMarker());
  if (json["one-of"].isArray()) {
    std::vector<std::string> options;
    for (const matjson::Value& option : json["one-of"]) {
      if (auto text = option.asString()) options.push_back(text.unwrap());
    }
    setting->m_impl->options = std::move(options);
  }
  auto value = ParseDefault<std::string>(json);
  if (!value) return Err(value.unwrapErr());
  REODE_FINISH_SETTING(setting, key, modID, value);
}

ZStringView StringSettingV3::getValue() const { return getValueRef(); }

Result<> StringSettingV3::isValid(std::string_view value) const {
  if (m_impl->options &&
      std::ranges::find(*m_impl->options, value) == m_impl->options->end()) {
    return Err("value is not one of the allowed options");
  }
  return Ok();
}

std::optional<std::vector<std::string>> StringSettingV3::getEnumOptions()
    const {
  return m_impl->options;
}
SettingNodeV3* StringSettingV3::createNode(float /*width*/) { return nullptr; }

class FileSettingV3::Impl {
 public:
  bool folder = false;
};

FileSettingV3::FileSettingV3(PrivateMarker)
    : m_impl(std::make_shared<Impl>()) {}

Result<std::shared_ptr<FileSettingV3>> FileSettingV3::parse(
    std::string key, std::string modID, matjson::Value const& json) {
  auto setting = std::make_shared<FileSettingV3>(PrivateMarker());
  setting->m_impl->folder = json["type"].asString().unwrapOr("") == "folder";
  auto text = reode::geode_shim::DefaultValue(json).asString();
  if (!text) return Err("\"default\" must be a path");
  // Geode expands {gd_dir}-style placeholders; Reode keeps the text as is.
  setting->init(std::move(key), std::move(modID));
  setting->setDefaultValue(
      std::filesystem::path(utils::string::utf8ToWide(text.unwrap())));
  setting->reset();
  return Ok(std::move(setting));
}

Result<> FileSettingV3::isValid(std::filesystem::path const& /*value*/) const {
  return Ok();
}
bool FileSettingV3::isFolder() const { return m_impl->folder; }
bool FileSettingV3::useSaveDialog() const { return false; }
SettingNodeV3* FileSettingV3::createNode(float /*width*/) { return nullptr; }

class Color3BSettingV3::Impl {};

Color3BSettingV3::Color3BSettingV3(PrivateMarker)
    : m_impl(std::make_shared<Impl>()) {}

Result<std::shared_ptr<Color3BSettingV3>> Color3BSettingV3::parse(
    std::string key, std::string modID, matjson::Value const& json) {
  auto setting = std::make_shared<Color3BSettingV3>(PrivateMarker());
  auto value = ParseDefault<cocos2d::ccColor3B>(json);
  if (!value) return Err(value.unwrapErr());
  REODE_FINISH_SETTING(setting, key, modID, value);
}

Result<> Color3BSettingV3::isValid(cocos2d::ccColor3B /*value*/) const {
  return Ok();
}
SettingNodeV3* Color3BSettingV3::createNode(float /*width*/) { return nullptr; }

class Color4BSettingV3::Impl {};

Color4BSettingV3::Color4BSettingV3(PrivateMarker)
    : m_impl(std::make_shared<Impl>()) {}

Result<std::shared_ptr<Color4BSettingV3>> Color4BSettingV3::parse(
    std::string key, std::string modID, matjson::Value const& json) {
  auto setting = std::make_shared<Color4BSettingV3>(PrivateMarker());
  auto value = ParseDefault<cocos2d::ccColor4B>(json);
  if (!value) return Err(value.unwrapErr());
  REODE_FINISH_SETTING(setting, key, modID, value);
}

#undef REODE_FINISH_SETTING

Result<> Color4BSettingV3::isValid(cocos2d::ccColor4B /*value*/) const {
  return Ok();
}
SettingNodeV3* Color4BSettingV3::createNode(float /*width*/) { return nullptr; }

// --- Exported template members ----------------------------------------------

// Mods import these from Geode.dll because the setting classes deriving
// from the template are DLL classes in Geode.
template bool SettingBaseValueV3<bool, bool>::getValue() const;
template int64_t SettingBaseValueV3<int64_t, int64_t>::getValue() const;
template double SettingBaseValueV3<double, double>::getValue() const;
template std::string
SettingBaseValueV3<std::string, std::string_view>::getValue() const;
template std::filesystem::path SettingBaseValueV3<
    std::filesystem::path, std::filesystem::path const&>::getValue() const;
template cocos2d::ccColor3B
SettingBaseValueV3<cocos2d::ccColor3B, cocos2d::ccColor3B>::getValue() const;
template cocos2d::ccColor4B
SettingBaseValueV3<cocos2d::ccColor4B, cocos2d::ccColor4B>::getValue() const;
template void SettingBaseValueV3<bool, bool>::setValue(bool);
template void SettingBaseValueV3<int64_t, int64_t>::setValue(int64_t);
template void SettingBaseValueV3<double, double>::setValue(double);
template void SettingBaseValueV3<std::string, std::string_view>::setValue(
    std::string_view);
template void SettingBaseValueV3<
    std::filesystem::path,
    std::filesystem::path const&>::setValue(std::filesystem::path const&);
template void SettingBaseValueV3<
    cocos2d::ccColor3B, cocos2d::ccColor3B>::setValue(cocos2d::ccColor3B);
template void SettingBaseValueV3<
    cocos2d::ccColor4B, cocos2d::ccColor4B>::setValue(cocos2d::ccColor4B);

// --- Events -----------------------------------------------------------------

SettingChangedEventV3::SettingChangedEventV3(Mod* mod, std::string settingKey)
    : GlobalEvent(std::string(mod->getID()), std::move(settingKey)) {}

KeybindSettingPressedEventV3::KeybindSettingPressedEventV3(
    Mod* mod, std::string settingKey)
    : GlobalEvent(std::string(mod->getID()), std::move(settingKey)) {}

}  // namespace geode
