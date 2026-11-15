// Copyright 2026 The Reode Authors
//
// Fast Transitions: makes the fades between menus and levels shorter.
//
// Every cocos2d scene transition, whatever its kind, runs
// CCTransitionScene::initWithDuration, so scaling the duration there covers
// them all.

#include <algorithm>

#include "mod_util.h"
#include "reode/reode.h"

namespace {

using reode::mod_util::CocosExport;
using InitWithDuration = bool (*)(void* self, float duration, void* scene);

constexpr char kInitWithDuration[] =
    "?initWithDuration@CCTransitionScene@cocos2d@@UEAA_NMPEAVCCScene@2@@Z";
// Shortest transition, so that the fade still starts and ends cleanly.
constexpr float kMinDuration = 0.01f;

constinit InitWithDuration g_init_with_duration = nullptr;
constinit float g_duration_percent = 0;

bool InitWithDurationDetour(void* self, float duration, void* scene) {
  const float scaled =
      std::max(kMinDuration, duration * g_duration_percent / 100.0f);
  return g_init_with_duration(self, scaled, scene);
}

}  // namespace

REODE_MOD_MAIN(mod) {
  reode::mod_util::Settings settings(mod.save_dir());
  // Percent of the original duration: 50 = twice as fast, 0 = instant.
  g_duration_percent =
      static_cast<float>(std::clamp(settings.GetInt(L"DurationPercent", 40),
                                    0, 100));

  auto target = CocosExport<void*>(kInitWithDuration);
  if (target == nullptr) {
    mod.Error("CCTransitionScene::initWithDuration not found in cocos2d");
    return std::unexpected(reode::Status::kNotFound);
  }
  auto hook =
      mod.Hook(target, &InitWithDurationDetour, &g_init_with_duration);
  if (!hook) {
    mod.Error("Hook failed: {}", reode::StatusToString(hook.error()));
    return std::unexpected(hook.error());
  }
  mod.Info("Transitions take {}% of their usual time (DurationPercent in "
           "settings.ini)",
           g_duration_percent);
  return {};
}
