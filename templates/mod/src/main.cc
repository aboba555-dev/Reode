// A minimal Reode mod: writes a line to the log every time the main menu
// opens. See docs/first-mod.md for a walkthrough.

#include <cstdint>

#include "reode/reode.h"

namespace {

// MenuLayer::init is in the same vtable slot as cocos2d::CCLayer::init.
constexpr std::uint32_t kInitSlot = 9;

using MenuLayerInit = bool (*)(void* self);

constinit MenuLayerInit g_original_init = nullptr;

bool MenuLayerInitDetour(void* self) {
  if (!g_original_init(self)) return false;
  reode::Mod::Get().Info("The main menu is open");
  return true;
}

}  // namespace

REODE_MOD_MAIN(mod) {
  auto init = reode::game::FindVirtual("MenuLayer", kInitSlot);
  if (!init) {
    mod.Error("MenuLayer::init not found");
    return std::unexpected(init.error());
  }
  auto hook = mod.Hook(*init, &MenuLayerInitDetour, &g_original_init);
  if (!hook) {
    mod.Error("Hook failed: {}", reode::StatusToString(hook.error()));
    return std::unexpected(hook.error());
  }
  mod.Info("{} {} is ready", mod.id(), mod.version());
  return {};
}
