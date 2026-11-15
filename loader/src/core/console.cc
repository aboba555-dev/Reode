// Copyright 2026 The Reode Authors

#include "core/console.h"

#include <windows.h>

#include <string_view>

namespace reode {
namespace {

constinit HANDLE g_output = nullptr;

}  // namespace

bool OpenDebugConsole() {
  if (g_output != nullptr) return true;
  if (!AllocConsole() && GetConsoleWindow() == nullptr) return false;

  SetConsoleTitleW(L"Reode Debug Console");
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);

  g_output = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                         OPEN_EXISTING, 0, nullptr);
  if (g_output == INVALID_HANDLE_VALUE) {
    g_output = nullptr;
    return false;
  }

  DWORD output_mode = 0;
  if (GetConsoleMode(g_output, &output_mode)) {
    SetConsoleMode(g_output, output_mode | ENABLE_PROCESSED_OUTPUT |
                                 ENABLE_VIRTUAL_TERMINAL_PROCESSING);
  }

  HANDLE input = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                             OPEN_EXISTING, 0, nullptr);
  if (input != INVALID_HANDLE_VALUE) {
    DWORD input_mode = 0;
    if (GetConsoleMode(input, &input_mode)) {
      input_mode &= ~ENABLE_QUICK_EDIT_MODE;
      SetConsoleMode(input, input_mode | ENABLE_EXTENDED_FLAGS);
    }
    CloseHandle(input);
  }

  if (HWND window = GetConsoleWindow(); window != nullptr) {
    if (HMENU menu = GetSystemMenu(window, FALSE); menu != nullptr) {
      DeleteMenu(menu, SC_CLOSE, MF_BYCOMMAND);
    }
  }
  return true;
}

void WriteToConsole(std::string_view utf8_text) {
  if (g_output == nullptr) return;
  DWORD written = 0;
  WriteFile(g_output, utf8_text.data(), static_cast<DWORD>(utf8_text.size()),
            &written, nullptr);
}

}  // namespace reode
