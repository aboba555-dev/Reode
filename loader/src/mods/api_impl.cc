// Copyright 2026 The Reode Authors

#include "mods/api_impl.h"

#include <string>
#include <utility>

#include "core/logger.h"
#include "game/module_image.h"
#include "game/pattern.h"
#include "game/rtti.h"
#include "hook/hook_manager.h"
#include "mods/mod_manager.h"

namespace reode::mods {
namespace {

std::string* GameVersionStorage() {
  static std::string* const kVersion = new std::string("unknown");
  return kVersion;
}

const char* ModName(ReodeMod* mod) {
  return mod != nullptr && mod->mod != nullptr ? mod->mod->metadata.id.c_str()
                                               : "Reode";
}

void ApiLog(ReodeMod* mod, ReodeLogLevel level, const char* message) {
  if (message == nullptr) return;
  auto severity = LogSeverity::kInfo;
  switch (level) {
    case kReodeLogDebug:
      severity = LogSeverity::kDebug;
      break;
    case kReodeLogInfo:
      severity = LogSeverity::kInfo;
      break;
    case kReodeLogWarning:
      severity = LogSeverity::kWarning;
      break;
    case kReodeLogError:
      severity = LogSeverity::kError;
      break;
  }
  Logger::Instance().Write(severity, ModName(mod), message);
}

ReodeStatus ApiCreateHook(ReodeMod* mod, void* target, void* detour,
                          int32_t priority, void** original,
                          ReodeHook** out_hook) {
  auto hook = hook::HookManager::Instance().Create(target, detour, priority,
                                                   ModName(mod), original);
  if (!hook) {
    LogWarning("[{}] hook on {} failed (status {})", ModName(mod), target,
               static_cast<int>(hook.error()));
    return hook.error();
  }
  if (out_hook != nullptr) *out_hook = reinterpret_cast<ReodeHook*>(*hook);
  return kReodeOk;
}

ReodeStatus ApiSetHookEnabled(ReodeHook* hook, int32_t enabled) {
  return hook::HookManager::Instance().SetEnabled(
      reinterpret_cast<hook::HookManager::Hook*>(hook), enabled != 0);
}

ReodeStatus ApiRemoveHook(ReodeHook* hook) {
  return hook::HookManager::Instance().Remove(
      reinterpret_cast<hook::HookManager::Hook*>(hook));
}

void* ApiGameBase() {
  return reinterpret_cast<void*>(game::ModuleImage::Game().base());
}

const char* ApiGameVersion() { return GameVersionStorage()->c_str(); }

void* ApiFindVTable(const char* class_name, uint32_t subobject_offset) {
  if (class_name == nullptr) return nullptr;
  return reinterpret_cast<void*>(game::FindVTable(
      game::ModuleImage::Game(), class_name, subobject_offset));
}

void* ApiFindVirtual(const char* class_name, uint32_t slot) {
  if (class_name == nullptr) return nullptr;
  return reinterpret_cast<void*>(
      game::FindVirtualFunction(game::ModuleImage::Game(), class_name, slot));
}

void* ApiFindPattern(const char* pattern_text) {
  if (pattern_text == nullptr) return nullptr;
  auto pattern = game::Pattern::Parse(pattern_text);
  auto text = game::ModuleImage::Game().FindSection(".text");
  if (!pattern || !text) return nullptr;
  auto offset = pattern->FindIn(text->bytes());
  if (!offset) return nullptr;
  return reinterpret_cast<void*>(text->begin + *offset);
}

const char* ApiModId(ReodeMod* mod) { return ModName(mod); }

const char* ApiModVersion(ReodeMod* mod) {
  return mod != nullptr && mod->mod != nullptr ? mod->mod->version_text.c_str()
                                               : "";
}

const char* ApiModSaveDir(ReodeMod* mod) {
  return mod != nullptr && mod->mod != nullptr ? mod->mod->save_dir.c_str()
                                               : "";
}

constinit const ReodeApi kApi = {
    .version = REODE_API_VERSION,
    .size = sizeof(ReodeApi),
    .log = &ApiLog,
    .create_hook = &ApiCreateHook,
    .set_hook_enabled = &ApiSetHookEnabled,
    .remove_hook = &ApiRemoveHook,
    .game_base = &ApiGameBase,
    .game_version = &ApiGameVersion,
    .find_vtable = &ApiFindVTable,
    .find_virtual = &ApiFindVirtual,
    .find_pattern = &ApiFindPattern,
    .mod_id = &ApiModId,
    .mod_version = &ApiModVersion,
    .mod_save_dir = &ApiModSaveDir,
};

}  // namespace

void SetGameVersionForApi(std::string version) {
  *GameVersionStorage() = std::move(version);
}

const ReodeApi* GetApiTable() { return &kApi; }

}  // namespace reode::mods
