// Copyright 2026 The Reode Authors
//
// Diagnostic mod: logs sprite frame lookups made by
// CCSpriteFrameCache::spriteFrameByName, flagging names that are not found.

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <string_view>

#include "reode/reode.h"

namespace {

constexpr int kMaxLoggedLookups = 60;

using SpriteFrameByName = void* (*)(void* cache, const char* name);

constinit SpriteFrameByName g_original = nullptr;
constinit std::atomic<int> g_logged = 0;

void* SpriteFrameByNameDetour(void* cache, const char* name) {
  void* frame = g_original(cache, name);
  std::string_view text = name != nullptr ? name : "";
  const bool interesting = frame == nullptr ||
                           text.find("diff") != std::string_view::npos ||
                           text.find("god") != std::string_view::npos;
  if (interesting && g_logged.fetch_add(1) < kMaxLoggedLookups) {
    reode::Mod::Get().Info("spriteFrameByName(\"{}\") -> {}",
                           name != nullptr ? name : "<null>",
                           frame != nullptr ? "found" : "NOT FOUND");
  }
  return frame;
}

}  // namespace

REODE_MOD_MAIN(mod) {
  HMODULE cocos = GetModuleHandleW(L"libcocos2d.dll");
  void* target = reinterpret_cast<void*>(
      GetProcAddress(cocos,
                     "?spriteFrameByName@CCSpriteFrameCache@cocos2d@@"
                     "QEAAPEAVCCSpriteFrame@2@PEBD@Z"));
  if (target == nullptr) return std::unexpected(reode::Status::kNotFound);
  auto hook = mod.Hook(target, &SpriteFrameByNameDetour, &g_original);
  if (!hook) return std::unexpected(hook.error());
  return {};
}
