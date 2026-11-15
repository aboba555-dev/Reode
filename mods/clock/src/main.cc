// Copyright 2026 The Reode Authors
//
// Clock: shows the time of day and how long you have been playing in the
// bottom-right corner, on every screen. F9 shows or hides it.
//
// The label is drawn as cocos2d's "notification node", which CCDirector
// draws on top of the running scene every frame, so it needs no hook in any
// game class.

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <format>
#include <string>

#include "mod_util.h"
#include "reode/reode.h"

namespace {

using reode::mod_util::CocosExport;
using reode::mod_util::Point;
using reode::mod_util::Size;

// CCDirector.
using DrawScene = void (*)(void* director);
using SharedDirector = void* (*)();
using GetNode = void* (*)(void* director);
using SetNode = void (*)(void* director, void* node);
// Member functions return structs through a hidden pointer.
using GetWinSize = Size* (*)(void* director, Size* out);
// CCNode / CCLabelBMFont.
using CreateNode = void* (*)();
using CreateLabel = void* (*)(const char* text, const char* font);
using AddChild = void (*)(void* parent, void* child, int z_order);
using SetPoint = void (*)(void* node, const Point* point);
using SetFloat = void (*)(void* node, float value);
using SetBool = void (*)(void* node, bool value);
using SetText = void (*)(void* label, const char* text, bool update);
using Retain = void (*)(void* object);

struct Cocos {
  DrawScene draw_scene = CocosExport<DrawScene>(
      "?drawScene@CCDirector@cocos2d@@QEAAXXZ");
  SharedDirector shared_director = CocosExport<SharedDirector>(
      "?sharedDirector@CCDirector@cocos2d@@SAPEAV12@XZ");
  GetNode get_notification_node = CocosExport<GetNode>(
      "?getNotificationNode@CCDirector@cocos2d@@QEAAPEAVCCNode@2@XZ");
  SetNode set_notification_node = CocosExport<SetNode>(
      "?setNotificationNode@CCDirector@cocos2d@@QEAAXPEAVCCNode@2@@Z");
  GetWinSize get_win_size = CocosExport<GetWinSize>(
      "?getWinSize@CCDirector@cocos2d@@QEAA?AVCCSize@2@XZ");
  CreateNode create_node =
      CocosExport<CreateNode>("?create@CCNode@cocos2d@@SAPEAV12@XZ");
  CreateLabel create_label = CocosExport<CreateLabel>(
      "?create@CCLabelBMFont@cocos2d@@SAPEAV12@PEBD0@Z");
  AddChild add_child = CocosExport<AddChild>(
      "?addChild@CCNode@cocos2d@@UEAAXPEAV12@H@Z");
  SetPoint set_position = CocosExport<SetPoint>(
      "?setPosition@CCNode@cocos2d@@UEAAXAEBVCCPoint@2@@Z");
  SetPoint set_anchor_point = CocosExport<SetPoint>(
      "?setAnchorPoint@CCLabelBMFont@cocos2d@@UEAAXAEBVCCPoint@2@@Z");
  SetFloat set_scale =
      CocosExport<SetFloat>("?setScale@CCNode@cocos2d@@UEAAXM@Z");
  SetBool set_visible =
      CocosExport<SetBool>("?setVisible@CCNode@cocos2d@@UEAAX_N@Z");
  // Only functions that CCLabelBMFont declares itself, or that override a
  // CCNode function, take the label pointer as `this`. Overrides of
  // CCLabelProtocol or CCRGBAProtocol functions (setString(const char*),
  // setOpacity, setColor) expect a pointer to that base class instead.
  SetText set_string = CocosExport<SetText>(
      "?setString@CCLabelBMFont@cocos2d@@UEAAXPEBD_N@Z");
  Retain retain = CocosExport<Retain>("?retain@CCObject@cocos2d@@QEAAXXZ");

  bool complete() const {
    return draw_scene && shared_director && get_notification_node &&
           set_notification_node && get_win_size && create_node &&
           create_label && add_child && set_position && set_anchor_point &&
           set_scale && set_visible && set_string && retain;
  }
};

struct Options {
  bool show_clock = true;
  bool show_session = true;
  float scale = 0.6f;
};

constexpr float kMargin = 6.0f;
constexpr auto kUpdateInterval = std::chrono::seconds(1);

constinit const Cocos* g_cocos = nullptr;
constinit DrawScene g_draw_scene = nullptr;
constinit reode::mod_util::KeyPress* g_key = nullptr;
constinit Options g_options;
constinit void* g_label = nullptr;
constinit bool g_visible = true;
std::chrono::steady_clock::time_point g_session_start;
std::chrono::steady_clock::time_point g_next_update;

std::string ClockText() {
  std::string text;
  if (g_options.show_clock) {
    const auto now = std::chrono::zoned_time(
        std::chrono::current_zone(),
        std::chrono::floor<std::chrono::minutes>(
            std::chrono::system_clock::now()));
    text = std::format("{:%H:%M}", now);
  }
  if (g_options.show_session) {
    const auto played = std::chrono::duration_cast<std::chrono::minutes>(
        std::chrono::steady_clock::now() - g_session_start);
    if (!text.empty()) text += "  |  ";
    text += std::format("played {}:{:02}", played.count() / 60,
                        played.count() % 60);
  }
  return text;
}

// Creates the label on the first frame, when cocos2d is fully set up.
void CreateClockLabel(void* director) {
  const Cocos& cocos = *g_cocos;
  g_label = cocos.create_label(ClockText().c_str(), "chatFont.fnt");
  if (g_label == nullptr) return;
  const Point anchor{1.0f, 0.0f};
  cocos.set_anchor_point(g_label, &anchor);
  cocos.set_scale(g_label, g_options.scale);
  cocos.set_visible(g_label, g_visible);

  // Shares the notification node with anything else that uses it.
  void* holder = cocos.get_notification_node(director);
  if (holder == nullptr) {
    holder = cocos.create_node();
    cocos.retain(holder);
    cocos.set_notification_node(director, holder);
  }
  cocos.add_child(holder, g_label, 0);
}

void DrawSceneDetour(void* director) {
  const Cocos& cocos = *g_cocos;
  if (g_label == nullptr) CreateClockLabel(director);
  if (g_label != nullptr) {
    if (g_key->Poll()) {
      g_visible = !g_visible;
      cocos.set_visible(g_label, g_visible);
      g_next_update = {};  // Show the current time right away.
    }
    const auto now = std::chrono::steady_clock::now();
    if (g_visible && now >= g_next_update) {
      g_next_update = now + kUpdateInterval;
      cocos.set_string(g_label, ClockText().c_str(), true);
      // The window can be resized at any time.
      Size win_size;
      cocos.get_win_size(director, &win_size);
      const Point position{win_size.width - kMargin, kMargin};
      cocos.set_position(g_label, &position);
    }
  }
  g_draw_scene(director);
}

}  // namespace

REODE_MOD_MAIN(mod) {
  static const Cocos cocos;
  if (!cocos.complete()) {
    mod.Error("A cocos2d function this mod needs is missing");
    return std::unexpected(reode::Status::kNotFound);
  }
  g_cocos = &cocos;

  reode::mod_util::Settings settings(mod.save_dir());
  g_options.show_clock = settings.GetBool(L"ShowClock", true);
  g_options.show_session = settings.GetBool(L"ShowPlayTime", true);
  g_options.scale =
      static_cast<float>(std::clamp(settings.GetInt(L"SizePercent", 60), 20,
                                    200)) /
      100.0f;
  g_visible = settings.GetBool(L"VisibleAtStart", true);
  // Virtual-key code; 120 = F9.
  static reode::mod_util::KeyPress key(settings.GetInt(L"ToggleKey", VK_F9));
  g_key = &key;
  g_session_start = std::chrono::steady_clock::now();

  auto hook = mod.Hook(reinterpret_cast<void*>(cocos.draw_scene),
                       &DrawSceneDetour, &g_draw_scene);
  if (!hook) {
    mod.Error("Hook failed: {}", reode::StatusToString(hook.error()));
    return std::unexpected(hook.error());
  }
  return {};
}
