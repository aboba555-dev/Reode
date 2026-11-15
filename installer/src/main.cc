// Copyright 2026 The Reode Authors
//
// ReodeSetup.exe: installs, updates and removes Reode with a few clicks.
// The Reode binaries travel inside the executable as RCDATA resources.

#include <windows.h>
#include <commctrl.h>
#include <shobjidl.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>

#include "resource_ids.h"
#include "setup.h"

#pragma comment(linker, "\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace {

namespace fs = std::filesystem;
using reode::setup::Payload;

constexpr wchar_t kTitle[] = L"Reode Setup";

enum ButtonId : int {
  kInstall = 100,
  kUninstall,
  kChooseFolder,
};

std::wstring Widen(std::string_view text) {
  if (text.empty()) return {};
  int size = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                 static_cast<int>(text.size()), nullptr, 0);
  std::wstring wide(static_cast<std::size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                      wide.data(), size);
  return wide;
}

std::string_view Resource(int id) {
  HRSRC info = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
  if (info == nullptr) return {};
  HGLOBAL data = LoadResource(nullptr, info);
  if (data == nullptr) return {};
  return {static_cast<const char*>(LockResource(data)),
          SizeofResource(nullptr, info)};
}

void Message(HWND owner, PCWSTR icon, const std::wstring& heading,
             const std::wstring& text) {
  TaskDialog(owner, nullptr, kTitle, heading.c_str(), text.c_str(),
             TDCBF_OK_BUTTON, icon, nullptr);
}

std::optional<fs::path> PickFolder() {
  IFileOpenDialog* dialog = nullptr;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                              IID_PPV_ARGS(&dialog)))) {
    return std::nullopt;
  }
  std::optional<fs::path> result;
  DWORD options = 0;
  dialog->GetOptions(&options);
  dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
  dialog->SetTitle(L"Choose the Geometry Dash folder");
  IShellItem* item = nullptr;
  if (SUCCEEDED(dialog->Show(nullptr)) &&
      SUCCEEDED(dialog->GetResult(&item))) {
    PWSTR path = nullptr;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
      result = fs::path(path);
      CoTaskMemFree(path);
    }
    item->Release();
  }
  dialog->Release();
  return result;
}

// Asks for the game folder until a valid one is chosen or the user cancels.
std::optional<fs::path> AskForGameDir() {
  for (;;) {
    std::optional<fs::path> dir = PickFolder();
    if (!dir) return std::nullopt;
    if (reode::setup::IsGameDir(*dir)) return dir;
    Message(nullptr, TD_WARNING_ICON, L"That is not the Geometry Dash folder",
            L"Choose the folder that contains GeometryDash.exe.");
  }
}

// The main page. Returns the button that was pressed.
int AskAction(const fs::path& game_dir, bool installed) {
  std::wstring content =
      std::format(L"Geometry Dash folder:\n{}\n\n{}", game_dir.wstring(),
                  installed ? L"Reode is installed. Installing again updates "
                              L"it; your mods and settings stay."
                            : L"Reode is not installed yet. Your save files "
                              L"are backed up the first time the game starts "
                              L"with Reode.");
  TASKDIALOG_BUTTON buttons[3];
  UINT count = 0;
  buttons[count++] = {kInstall, installed ? L"Update Reode" : L"Install Reode"};
  if (installed) buttons[count++] = {kUninstall, L"Uninstall Reode"};
  buttons[count++] = {kChooseFolder, L"Choose another folder"};

  TASKDIALOGCONFIG config = {.cbSize = sizeof(config)};
  config.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ALLOW_DIALOG_CANCELLATION;
  config.pszWindowTitle = kTitle;
  config.pszMainIcon = TD_INFORMATION_ICON;
  config.pszMainInstruction = L"Reode, a mod manager for Geometry Dash";
  config.pszContent = content.c_str();
  config.pButtons = buttons;
  config.cButtons = count;
  config.dwCommonButtons = TDCBF_CLOSE_BUTTON;
  int pressed = IDCLOSE;
  TaskDialogIndirect(&config, &pressed, nullptr, nullptr);
  return pressed;
}

// Returns IDYES (delete mods too), IDNO (keep them) or IDCANCEL.
int AskRemoveData() {
  TASKDIALOG_BUTTON buttons[] = {
      {IDNO, L"Keep my mods and settings"},
      {IDYES, L"Delete mods, settings and logs too"},
  };
  TASKDIALOGCONFIG config = {.cbSize = sizeof(config)};
  config.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ALLOW_DIALOG_CANCELLATION;
  config.pszWindowTitle = kTitle;
  config.pszMainIcon = TD_WARNING_ICON;
  config.pszMainInstruction = L"Uninstall Reode?";
  config.pszContent =
      L"The game goes back to how it was before Reode. Save backups in "
      L"reode\\backups are always kept.";
  config.pButtons = buttons;
  config.cButtons = ARRAYSIZE(buttons);
  config.dwCommonButtons = TDCBF_CANCEL_BUTTON;
  int pressed = IDCANCEL;
  TaskDialogIndirect(&config, &pressed, nullptr, nullptr);
  return pressed;
}

bool EnsureGameClosed() {
  while (reode::setup::IsGameRunning()) {
    int pressed = IDCANCEL;
    TaskDialog(nullptr, nullptr, kTitle, L"Geometry Dash is running",
               L"Close the game, then press Retry.",
               TDCBF_RETRY_BUTTON | TDCBF_CANCEL_BUTTON, TD_WARNING_ICON,
               &pressed);
    if (pressed != IDRETRY) return false;
  }
  return true;
}

int Run() {
  const Payload payload = {
      .reode_dll = Resource(REODE_RESOURCE_REODE_DLL),
      .proxy_dll = Resource(REODE_RESOURCE_PROXY_DLL),
      .geode_dll = Resource(REODE_RESOURCE_GEODE_DLL),
      .mods = {{L"reode.fast-transitions.reode",
                Resource(REODE_RESOURCE_MOD_FAST_TRANSITIONS)},
               {L"reode.screenshot.reode",
                Resource(REODE_RESOURCE_MOD_SCREENSHOT)},
               {L"reode.clock.reode", Resource(REODE_RESOURCE_MOD_CLOCK)}},
  };
  // geode_dll is empty in builds without REODE_GEODE_COMPAT.
  if (payload.reode_dll.empty() || payload.proxy_dll.empty() ||
      std::ranges::any_of(payload.mods,
                          [](const Payload::Mod& mod) { return mod.data.empty(); })) {
    Message(nullptr, TD_ERROR_ICON, L"This installer is damaged",
            L"Download ReodeSetup.exe again.");
    return 1;
  }

  std::optional<fs::path> game_dir = reode::setup::FindSteamGameDir();
  if (!game_dir) {
    Message(nullptr, TD_INFORMATION_ICON,
            L"Geometry Dash was not found automatically",
            L"Choose the folder that contains GeometryDash.exe.");
    game_dir = AskForGameDir();
    if (!game_dir) return 0;
  }

  for (;;) {
    const bool installed = reode::setup::IsReodeInstalled(*game_dir);
    switch (AskAction(*game_dir, installed)) {
      case kChooseFolder:
        if (auto dir = AskForGameDir()) game_dir = dir;
        continue;
      case kInstall: {
        if (!EnsureGameClosed()) continue;
        auto result = reode::setup::Install(*game_dir, payload);
        if (!result) {
          Message(nullptr, TD_ERROR_ICON, L"Installation failed",
                  Widen(result.error()));
          continue;
        }
        Message(nullptr, TD_INFORMATION_ICON,
                installed ? L"Reode is updated" : L"Reode is installed",
                L"Start Geometry Dash. The Reode button is on the left side "
                L"of the main menu. Three mods come with Reode: Fast "
                L"Transitions, Screenshot (F12) and Clock (F9). Put more "
                L"mods (.reode files) into the reode\\mods folder of the "
                L"game.\n\nHold Shift while the game starts to run it "
                L"without mods.");
        return 0;
      }
      case kUninstall: {
        const int remove = AskRemoveData();
        if (remove == IDCANCEL || !EnsureGameClosed()) continue;
        auto result = reode::setup::Uninstall(*game_dir, remove == IDYES);
        if (!result) {
          Message(nullptr, TD_ERROR_ICON, L"Uninstall failed",
                  Widen(result.error()));
          continue;
        }
        Message(nullptr, TD_INFORMATION_ICON, L"Reode is uninstalled",
                L"Geometry Dash starts without Reode again.");
        return 0;
      }
      default:
        return 0;
    }
  }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE /*instance*/, HINSTANCE /*previous*/,
                    PWSTR /*command_line*/, int /*show*/) {
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  int code = Run();
  CoUninitialize();
  return code;
}
