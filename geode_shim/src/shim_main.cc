// Copyright 2026 The Reode Authors
//
// Reode's Geode.dll: entry points used by Reode.dll (see
// include/reode/geode_shim_host.h). The Geode API itself is implemented in
// the geode_*.cc files.

#include <windows.h>

#include <cstdlib>
#include <filesystem>
#include <format>
#include <string>
#include <utility>
#include <vector>

#include "Geode/loader/Mod.hpp"
#include "Geode/loader/ModMetadata.hpp"
#include "geode_settings.h"
#include "host.h"
#include "main_thread.h"
#include "matjson.hpp"
#include "mod_registry.h"
#include "reode/geode_shim_host.h"

namespace {

std::filesystem::path PathFromUtf8(const char* text) {
  if (text == nullptr) return {};
  std::string value(text);
  return std::filesystem::path(std::u8string(value.begin(), value.end()));
}

}  // namespace

// The opaque handle is the geode::Mod itself.
struct ReodeGeodeMod {};

extern "C" {

__declspec(dllexport) const char* ReodeGeodeShimVersion() {
  return REODE_GEODE_SHIM_SDK_VERSION;
}

__declspec(dllexport) ReodeStatus
ReodeGeodeShimInitialize(const ReodeApi* api) {
  if (api == nullptr || api->version != REODE_API_VERSION ||
      api->size < sizeof(ReodeApi)) {
    return kReodeIncompatible;
  }
  reode::geode_shim::SetApi(api);
  reode::geode_shim::InstallMainThreadQueue();
  // Runs when the game exits normally, while the CRT is still alive.
  std::atexit([] { reode::geode_shim::ModRegistry::Instance().SaveAll(); });
  reode::geode_shim::Log(
      nullptr, kReodeLogInfo,
      std::format("Geode compatibility layer ready (Geode SDK {} ABI)",
                  REODE_GEODE_SHIM_SDK_VERSION));
  return kReodeOk;
}

__declspec(dllexport) ReodeGeodeMod* ReodeGeodeShimBeginMod(
    ReodeMod* reode_mod, const char* mod_json, const char* resources_dir,
    const char* save_dir, uint32_t /*flags*/) {
  if (reode_mod == nullptr || mod_json == nullptr) return nullptr;
  auto parsed = matjson::parse(mod_json);
  if (!parsed) {
    reode::geode_shim::Log(reode_mod, kReodeLogError,
                           "mod.json is not valid JSON");
    return nullptr;
  }
  matjson::Value json = std::move(parsed).unwrap();

  reode::geode_shim::ModState state;
  state.reode_mod = reode_mod;
  state.id = json["id"].asString().unwrapOr("");
  state.name = json["name"].asString().unwrapOr(state.id);
  state.version = json["version"].asString().unwrapOr("");
  state.resources_dir = PathFromUtf8(resources_dir);
  state.save_dir = PathFromUtf8(save_dir);
  state.settings = reode::geode_shim::CreateSettings(json, state.id, reode_mod);
  for (const matjson::Value& developer : json["developers"]) {
    if (auto name = developer.asString())
      state.developers.push_back(name.unwrap());
  }
  if (auto developer = json["developer"].asString()) {
    state.developers.push_back(developer.unwrap());
  }

  geode::ModMetadata metadata(state.id);
  metadata.setName(state.name);
  metadata.setVersion(
      geode::VersionInfo::parse(state.version).unwrapOr(geode::VersionInfo()));
  metadata.setDevelopers(state.developers);
  metadata.setBinaryName(state.id + ".dll");
  if (auto text = json["description"].asString()) {
    metadata.setDescription(text.unwrap());
  }
  if (auto text = json["repository"].asString()) {
    metadata.setRepository(text.unwrap());
  }
  metadata.setNeedsEarlyLoad(json["early-load"].asBool().unwrapOr(false));
  metadata.setIsAPI(json["api"].isObject());
  if (auto text = json["geode"].asString()) {
    metadata.setGeodeVersion(geode::VersionInfo::parse(text.unwrap())
                                 .unwrapOr(geode::VersionInfo()));
  }
  std::vector<std::string> sheets;
  if (json["resources"]["spritesheets"].isObject()) {
    for (const auto& [name, files] : json["resources"]["spritesheets"]) {
      sheets.push_back(name);
    }
  }
  metadata.setSpritesheets(std::move(sheets));
  std::vector<std::pair<std::string, matjson::Value>> settings_json;
  if (json["settings"].isObject()) {
    for (const auto& [key, value] : json["settings"]) {
      settings_json.emplace_back(key, value);
    }
  }
  metadata.setSettings(std::move(settings_json));

  geode::Mod* mod = reode::geode_shim::ModRegistry::Instance().Begin(
      std::move(state), metadata);
  return reinterpret_cast<ReodeGeodeMod*>(mod);
}

__declspec(dllexport) ReodeStatus ReodeGeodeShimFinishMod(ReodeGeodeMod* mod,
                                                          void* module) {
  if (mod == nullptr || module == nullptr) return kReodeInvalidArgument;
  return reode::geode_shim::ModRegistry::Instance().Finish(
      reinterpret_cast<geode::Mod*>(mod), static_cast<HMODULE>(module));
}

__declspec(dllexport) void ReodeGeodeShimAbortMod(ReodeGeodeMod* mod) {
  reode::geode_shim::ModRegistry::Instance().Abort(
      reinterpret_cast<geode::Mod*>(mod));
}

}  // extern "C"

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID /*reserved*/) {
  if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(module);
  return TRUE;
}
