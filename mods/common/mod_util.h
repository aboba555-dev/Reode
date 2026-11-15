// Copyright 2026 The Reode Authors
//
// Small helpers shared by Reode's own mods. They call cocos2d only through
// functions exported by libcocos2d.dll, looked up by their decorated names,
// so they never depend on the memory layout of game or cocos2d classes.

#ifndef REODE_MODS_COMMON_MOD_UTIL_H_
#define REODE_MODS_COMMON_MOD_UTIL_H_

#include <windows.h>

#include <climits>
#include <filesystem>
#include <string>
#include <string_view>

#include "reode/reode.h"

namespace reode::mod_util {

// cocos2d's CCPoint, CCSize and ccColor3B, which are plain structs.
struct Point {
  float x = 0;
  float y = 0;
};
struct Size {
  float width = 0;
  float height = 0;
};
struct Color3B {
  unsigned char r = 0;
  unsigned char g = 0;
  unsigned char b = 0;
};

// An export of libcocos2d.dll, or nullptr.
template <typename Fn>
Fn CocosExport(const char* decorated_name) {
  HMODULE cocos = GetModuleHandleW(L"libcocos2d.dll");
  if (cocos == nullptr) return nullptr;
  return reinterpret_cast<Fn>(GetProcAddress(cocos, decorated_name));
}

// Settings stored as an .ini file in the mod's save directory. A missing
// key is written with its default, so players can find and edit it.
class Settings {
 public:
  explicit Settings(std::string_view save_dir)
      : file_(std::filesystem::path(
                  std::u8string(save_dir.begin(), save_dir.end())) /
              L"settings.ini") {}

  int GetInt(const wchar_t* key, int default_value) const {
    constexpr int kMissing = INT_MIN;
    int value = static_cast<int>(GetPrivateProfileIntW(
        kSection, key, kMissing, file_.c_str()));
    if (value != kMissing) return value;
    WritePrivateProfileStringW(kSection, key,
                               std::to_wstring(default_value).c_str(),
                               file_.c_str());
    return default_value;
  }

  bool GetBool(const wchar_t* key, bool default_value) const {
    return GetInt(key, default_value ? 1 : 0) != 0;
  }

  const std::filesystem::path& file() const { return file_; }

 private:
  static constexpr wchar_t kSection[] = L"settings";
  std::filesystem::path file_;
};

// Whether the game window has keyboard focus, so that hotkeys do not fire
// while the player types somewhere else.
inline bool GameHasFocus() {
  HWND window = GetForegroundWindow();
  DWORD process = 0;
  GetWindowThreadProcessId(window, &process);
  return process == GetCurrentProcessId();
}

// Reports each press of a key once, by polling (call it every frame).
class KeyPress {
 public:
  explicit KeyPress(int virtual_key) : key_(virtual_key) {}

  bool Poll() {
    const bool down = (GetAsyncKeyState(key_) & 0x8000) != 0;
    const bool pressed = down && !was_down_ && GameHasFocus();
    was_down_ = down;
    return pressed;
  }

 private:
  int key_;
  bool was_down_ = false;
};

}  // namespace reode::mod_util

#endif  // REODE_MODS_COMMON_MOD_UTIL_H_
