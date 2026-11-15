// Copyright 2026 The Reode Authors

#ifndef REODE_GEODE_SHIM_SRC_GEODE_SETTINGS_H_
#define REODE_GEODE_SHIM_SRC_GEODE_SETTINGS_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "matjson.hpp"
#include "reode/api.h"

namespace geode {
class SettingV3;
}  // namespace geode

namespace reode::geode_shim {

// Creates the settings declared in the "settings" object of a mod.json.
// Unsupported setting types are skipped with a warning.
std::vector<std::pair<std::string, std::shared_ptr<geode::SettingV3>>>
CreateSettings(const matjson::Value& mod_json, const std::string& mod_id,
               ReodeMod* reode_mod);

}  // namespace reode::geode_shim

#endif  // REODE_GEODE_SHIM_SRC_GEODE_SETTINGS_H_
