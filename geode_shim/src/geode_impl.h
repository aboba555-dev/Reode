// Copyright 2026 The Reode Authors
//
// Definitions of the private implementation classes declared by the Geode
// SDK headers: Reode's own state structs plus the Geode-typed members that
// only geode::Mod and geode::Hook member functions need.

#ifndef REODE_GEODE_SHIM_SRC_GEODE_IMPL_H_
#define REODE_GEODE_SHIM_SRC_GEODE_IMPL_H_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "Geode/loader/Hook.hpp"
#include "Geode/loader/Mod.hpp"
#include "Geode/loader/ModMetadata.hpp"
#include "Geode/loader/SettingV3.hpp"
#include "matjson.hpp"
#include "mod_registry.h"

namespace geode {

class Mod::Impl : public reode::geode_shim::ModState {
 public:
  explicit Impl(reode::geode_shim::ModState state)
      : reode::geode_shim::ModState(std::move(state)) {}

  ModMetadata metadata;
  // Mod::getSaveContainer(), persisted to <save dir>/saved.json.
  matjson::Value save_container = matjson::Value::object();
  // Setting values, persisted to <save dir>/settings.json.
  matjson::Value saved_settings = matjson::Value::object();
  std::map<std::string, SettingGenerator, std::less<>> custom_settings;
  bool logging_enabled = true;
  int log_level = Severity::Debug;
};

class Hook::Impl : public reode::geode_shim::HookState {
 public:
  explicit Impl(reode::geode_shim::HookState state)
      : reode::geode_shim::HookState(std::move(state)) {}
};

}  // namespace geode

#endif  // REODE_GEODE_SHIM_SRC_GEODE_IMPL_H_
