// Copyright 2026 The Reode Authors
//
// Reode.dll entry point. The XInput1_4.dll proxy calls ReodeInitialize() on
// the game's main thread, before the game's own CRT and main() run.

#include <windows.h>

#include <cstdlib>
#include <string>

#include "core/config.h"
#include "core/console.h"
#include "core/logger.h"
#include "core/paths.h"
#include "core/safety.h"
#include "game/game_version.h"
#include "game/menu_button.h"
#include "game/module_image.h"
#include "mods/api_impl.h"
#include "mods/mod_manager.h"
#include "reode/api.h"

#ifndef REODE_VERSION_STRING
#define REODE_VERSION_STRING "0.0.0"
#endif

namespace reode {
namespace {

constinit bool g_initialized = false;

// Holding Shift while the game starts loads no mods.
bool ShiftHeld() { return (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0; }

// Decides whether to start without mods and marks this session as running.
void SetUpSafeMode(const Paths& paths, const Config& config) {
  static const SessionMarker marker(paths.session_file);
  const bool crashed = marker.PreviousSessionCrashed();
  marker.Start();
  // Runs when the game exits normally, not after a crash or a kill.
  std::atexit([] { marker.End(); });

  auto& manager = mods::ModManager::Instance();
  if (ShiftHeld()) {
    manager.EnterSafeMode("Shift was held while the game started");
  } else if (crashed && config.safe_mode_after_crash) {
    manager.EnterSafeMode(
        "the game did not close normally last time. Restart it to load mods "
        "again");
  }
}

void Initialize() {
  const Paths& paths = GetPaths();
  Config config = LoadConfig(paths.config_file);
  SaveConfig(paths.config_file, config);

  Logger& logger = Logger::Instance();
  logger.set_min_severity(config.log_level);
  logger.OpenFile(paths.logs_dir);
  if (config.show_console && OpenDebugConsole()) {
    logger.set_console_enabled(true);
  }

  const game::ModuleImage& image = game::ModuleImage::Game();
  const std::string game_version =
      game::DetectGameVersion(image.timestamp(), config.game_version);
  mods::SetGameVersionForApi(game_version);

  LogInfo("Reode {} (API {})", REODE_VERSION_STRING, REODE_API_VERSION);
  LogInfo("Game directory: {}", PathToUtf8(paths.game_dir));
  LogInfo("GeometryDash.exe at {:#x}, PE timestamp {}, version {}",
          image.base(), image.timestamp(), game_version);

  // Before anything else touches the saves, mods included.
  BackupSavesOnStartup(paths.backups_dir, config.save_backups);
  SetUpSafeMode(paths, config);

  if (game::InstallMenuButton()) {
    LogInfo("Main menu button installed");
  }
  mods::ModManager::Instance().LoadAll(config, game_version);
}

}  // namespace
}  // namespace reode

extern "C" __declspec(dllexport) void ReodeInitialize(
    HMODULE /*proxy_module*/) {
  if (reode::g_initialized) return;
  reode::g_initialized = true;
  reode::Initialize();
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID /*reserved*/) {
  if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(module);
  return TRUE;
}
