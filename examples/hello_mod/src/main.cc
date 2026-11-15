// Copyright 2026 The Reode Authors
//
// Minimal Reode mod: hooks MenuLayer::init and logs whenever the main menu
// is created.

#include <cstdint>

#include "reode/reode.h"

namespace {

// Slot of cocos2d::CCLayer::init in the primary vtable, which MenuLayer
// overrides.
constexpr std::uint32_t kInitSlot = 9;

using MenuLayerInit = bool (*)(void* self);

constinit MenuLayerInit g_menu_layer_init = nullptr;
constinit int g_open_count = 0;

bool MenuLayerInitDetour(void* self) {
  if (!g_menu_layer_init(self)) return false;
  ++g_open_count;
  reode::Mod::Get().Info("Main menu opened ({} times so far)", g_open_count);
  return true;
}

}  // namespace

REODE_MOD_MAIN(mod) {
  mod.Info("Hello from {} {} on Geometry Dash {}", mod.id(), mod.version(),
           reode::game::Version());

  auto init = reode::game::FindVirtual("MenuLayer", kInitSlot);
  if (!init) {
    mod.Error("MenuLayer::init not found");
    return std::unexpected(init.error());
  }
  auto hook = mod.Hook(*init, &MenuLayerInitDetour, &g_menu_layer_init);
  if (!hook) {
    mod.Error("Hook failed: {}", reode::StatusToString(hook.error()));
    return std::unexpected(hook.error());
  }
  return {};
}
