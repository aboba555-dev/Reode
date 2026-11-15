// Copyright 2026 The Reode Authors

#include "game/menu_button.h"

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

#include "core/logger.h"
#include "game/cocos.h"
#include "game/module_image.h"
#include "game/rtti.h"
#include "hook/hook_manager.h"
#include "mods/mod_manager.h"

namespace reode::game {
namespace {

using MenuLayerInit = bool (*)(cocos::Node* self);

constexpr char kCCLayerVTable[] = "??_7CCLayer@cocos2d@@6BCCNode@1@@";
constexpr char kCCLayerInit[] = "?init@CCLayer@cocos2d@@UEAA_NXZ";
constexpr int kMaxVTableSlots = 256;

constexpr int kOverlayZOrder = 1000;
constexpr float kButtonScale = 0.8f;
constexpr float kButtonMarginX = 40.0f;
// Raises the button above the player name label under the left-hand side.
constexpr float kButtonOffsetY = 15.0f;
constexpr float kCloseMargin = 30.0f;
// cocos2d menus use -128. The overlay must see touches before them so that
// it can swallow them while it is open.
constexpr int kOverlayTouchPriority = -500;
// Why a mod did not load, under its name in the mod list.
constexpr std::size_t kReasonLineWidth = 90;
constexpr std::size_t kReasonMaxLines = 2;

constinit MenuLayerInit g_original_init = nullptr;

// Finds CCLayer::init's slot in the exported CCLayer vtable.
int FindLayerInitSlot() {
  HMODULE cocos = GetModuleHandleW(L"libcocos2d.dll");
  if (cocos == nullptr) return -1;
  auto* const* vtable =
      reinterpret_cast<void* const*>(GetProcAddress(cocos, kCCLayerVTable));
  auto* init = reinterpret_cast<void*>(GetProcAddress(cocos, kCCLayerInit));
  if (vtable == nullptr || init == nullptr) return -1;
  for (int slot = 0; slot < kMaxVTableSlots; ++slot) {
    if (vtable[slot] == init) return slot;
  }
  return -1;
}

std::string_view StateName(mods::ModState state) {
  switch (state) {
    case mods::ModState::kPending:
      return "pending";
    case mods::ModState::kLoaded:
      return "loaded";
    case mods::ModState::kDisabled:
      return "disabled";
    case mods::ModState::kFailed:
      return "failed";
  }
  return "?";
}

// Breaks `text` into indented lines of at most `width` characters at spaces,
// keeping at most `max_lines` of them; a cut-off text ends with "...".
std::string WrapText(std::string_view text, std::size_t width,
                     std::size_t max_lines) {
  std::string result;
  std::size_t lines = 0;
  while (!text.empty() && lines < max_lines) {
    std::size_t length = text.size();
    if (length > width) {
      length = text.rfind(' ', width);
      if (length == std::string_view::npos || length == 0) length = width;
    }
    if (lines != 0) result += '\n';
    result += "    ";
    result += text.substr(0, length);
    text.remove_prefix(length);
    while (!text.empty() && text.front() == ' ') text.remove_prefix(1);
    ++lines;
  }
  if (!text.empty()) result += "...";
  return result;
}

std::string BuildModListText() {
  const auto& manager = mods::ModManager::Instance();
  std::string text;
  if (manager.safe_mode()) {
    text += std::format("SAFE MODE - mods are off: {}.\n\n",
                        manager.safe_mode_reason());
  }
  if (manager.mods().empty()) {
    return text +
           "No mods installed.\nPut .reode or .geode files into reode/mods.";
  }
  for (const auto& mod : manager.mods()) {
    text += std::format("{} {} - {}\n", mod->metadata.name, mod->version_text,
                        StateName(mod->state));
    if (mod->state != mods::ModState::kLoaded && !mod->error.empty() &&
        !manager.safe_mode()) {
      text += WrapText(mod->error, kReasonLineWidth, kReasonMaxLines) + "\n";
    }
  }
  return text;
}

// Menu callbacks. cocos2d invokes them as (target->*selector)(sender); the
// target is always the overlay layer.
void OnToggleOverlay(cocos::Object* target, cocos::Object* /*sender*/) {
  const cocos::Cocos* cocos = cocos::Cocos::Get();
  auto* overlay = reinterpret_cast<cocos::Node*>(target);
  const bool visible = !cocos->IsVisible(overlay);
  cocos->SetVisible(overlay, visible);
  LogDebug("Mod list {}", visible ? "opened" : "closed");
}

cocos::Node* CreateOverlay(const cocos::Cocos& cocos, cocos::Size win_size) {
  cocos::Node* overlay =
      cocos.CreateLayerColor({0, 0, 0, 200}, win_size.width, win_size.height);
  if (overlay == nullptr) return nullptr;

  if (cocos::Node* title = cocos.CreateLabel("Reode", "goldFont.fnt")) {
    cocos.SetPosition(title, {win_size.width / 2, win_size.height - 30});
    cocos.AddChild(overlay, title, 1);
  }
  std::string list = BuildModListText();
  if (cocos::Node* label = cocos.CreateLabel(list.c_str(), "chatFont.fnt")) {
    cocos.SetAnchorPoint(label, {0.5f, 1.0f});
    cocos.SetPosition(label, {win_size.width / 2, win_size.height - 60});
    cocos.AddChild(overlay, label, 1);
  }

  // The overlay's menu has a higher touch priority than every game menu and
  // contains a full-screen item without a callback, so while the overlay is
  // visible every touch lands on it. cocos2d menus ignore touches when any
  // ancestor is hidden, so the closed overlay blocks nothing.
  // CCMenu::itemForTouch returns the first hit in ascending z order, so the
  // close button (z 0) wins over the blocker (z 1).
  cocos::Node* close_sprite =
      cocos.CreateSpriteWithFrameName("GJ_closeBtn_001.png");
  cocos::Node* blocker = cocos.CreateMenuItem();
  cocos::Node* menu = cocos.CreateMenu();
  if (close_sprite != nullptr && blocker != nullptr && menu != nullptr) {
    cocos::Node* close = cocos.CreateMenuItemSprite(
        close_sprite, nullptr, reinterpret_cast<cocos::Object*>(overlay),
        &OnToggleOverlay);
    cocos.SetTouchPriority(menu, kOverlayTouchPriority);
    cocos.SetPosition(menu, {0, 0});
    // Top-right, away from the game's own exit button in the top-left.
    cocos.SetPosition(
        close, {win_size.width - kCloseMargin, win_size.height - kCloseMargin});
    cocos.SetContentSize(blocker, win_size);
    cocos.SetPosition(blocker, {win_size.width / 2, win_size.height / 2});
    cocos.AddChild(menu, close, 0);
    cocos.AddChild(menu, blocker, 1);
    cocos.AddChild(overlay, menu, 2);
  }
  cocos.SetVisible(overlay, false);
  return overlay;
}

void AddReodeButton(cocos::Node* menu_layer) {
  const cocos::Cocos* cocos = cocos::Cocos::Get();
  if (cocos == nullptr) return;
  const cocos::Size win_size = cocos->WinSize();

  cocos::Node* overlay = CreateOverlay(*cocos, win_size);
  cocos::Node* sprite = cocos->CreateSpriteWithFrameName("GJ_plainBtn_001.png");
  cocos::Node* menu = cocos->CreateMenu();
  if (overlay == nullptr || sprite == nullptr || menu == nullptr) {
    LogError("Could not create the Reode menu button");
    return;
  }

  if (cocos::Node* letter = cocos->CreateLabel("R", "bigFont.fnt")) {
    cocos::Size sprite_size = cocos->ContentSize(sprite);
    cocos->SetPosition(letter,
                       {sprite_size.width / 2, sprite_size.height / 2 + 2});
    cocos->AddChild(sprite, letter, 1);
  }
  cocos::Node* button = cocos->CreateMenuItemSprite(
      sprite, nullptr, reinterpret_cast<cocos::Object*>(overlay),
      &OnToggleOverlay);
  cocos->SetScale(button, kButtonScale);
  cocos->SetPosition(menu, {0, 0});
  cocos->SetPosition(button,
                     {kButtonMarginX, win_size.height / 2 + kButtonOffsetY});
  cocos->AddChild(menu, button, 0);
  cocos->AddChild(menu_layer, menu, 10);
  cocos->AddChild(menu_layer, overlay, kOverlayZOrder);
}

bool MenuLayerInitDetour(cocos::Node* self) {
  // The first point where cocos2d is fully initialized; also re-adds mod
  // spritesheets after the game reloads its graphics.
  // TODO(reode): load resources during the loading screen instead.
  mods::ModManager::Instance().LoadResources();
  if (!g_original_init(self)) return false;
  AddReodeButton(self);
  return true;
}

}  // namespace

bool InstallMenuButton() {
  int slot = FindLayerInitSlot();
  if (slot < 0) {
    LogError("CCLayer::init not found in libcocos2d.dll");
    return false;
  }
  std::uintptr_t init =
      FindVirtualFunction(ModuleImage::Game(), "MenuLayer", slot);
  if (init == 0) {
    LogError("MenuLayer vtable not found through RTTI");
    return false;
  }
  LogDebug("MenuLayer::init = GeometryDash.exe+{:#x} (vtable slot {})",
           init - ModuleImage::Game().base(), slot);

  auto hook = hook::HookManager::Instance().Create(
      reinterpret_cast<void*>(init),
      reinterpret_cast<void*>(&MenuLayerInitDetour),
      /*priority=*/0, "reode", reinterpret_cast<void**>(&g_original_init));
  if (!hook) {
    LogError("Hooking MenuLayer::init failed (status {})",
             static_cast<int>(hook.error()));
    return false;
  }
  return true;
}

}  // namespace reode::game
