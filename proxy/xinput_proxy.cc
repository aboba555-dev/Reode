// Copyright 2026 The Reode Authors
//
// Forwarding stubs for the system XInput1_4.dll. Every export resolves the
// real function lazily from the System32 copy and passes arguments through.

#include <windows.h>

#include <array>
#include <cstdint>

#include "proxy/entry_hook.h"

namespace reode::proxy {
namespace {

HMODULE LoadSystemXInput() {
  static HMODULE const kModule = [] {
    std::array<wchar_t, MAX_PATH> path{};
    UINT length = GetSystemDirectoryW(path.data(), MAX_PATH);
    if (length == 0 || length + 16 >= MAX_PATH) return HMODULE{nullptr};
    lstrcatW(path.data(), L"\\xinput1_4.dll");
    return LoadLibraryW(path.data());
  }();
  return kModule;
}

FARPROC Resolve(LPCSTR name_or_ordinal) {
  HMODULE module = LoadSystemXInput();
  if (module == nullptr) return nullptr;
  return GetProcAddress(module, name_or_ordinal);
}

template <typename Fn>
Fn ResolveAs(LPCSTR name_or_ordinal) {
  return reinterpret_cast<Fn>(Resolve(name_or_ordinal));
}

LPCSTR Ordinal(WORD ordinal) { return MAKEINTRESOURCEA(ordinal); }

}  // namespace
}  // namespace reode::proxy

// The XInput SDK header is deliberately not included: the exported names are
// remapped in xinput1_4.def, and the opaque pointer types below are all the
// forwarding code needs.
extern "C" {

DWORD WINAPI ProxyXInputGetState(DWORD user_index, void* state) {
  using Fn = DWORD(WINAPI*)(DWORD, void*);
  static Fn const kReal = reode::proxy::ResolveAs<Fn>("XInputGetState");
  return kReal ? kReal(user_index, state) : ERROR_DEVICE_NOT_CONNECTED;
}

DWORD WINAPI ProxyXInputSetState(DWORD user_index, void* vibration) {
  using Fn = DWORD(WINAPI*)(DWORD, void*);
  static Fn const kReal = reode::proxy::ResolveAs<Fn>("XInputSetState");
  return kReal ? kReal(user_index, vibration) : ERROR_DEVICE_NOT_CONNECTED;
}

DWORD WINAPI ProxyXInputGetCapabilities(DWORD user_index, DWORD flags,
                                        void* capabilities) {
  using Fn = DWORD(WINAPI*)(DWORD, DWORD, void*);
  static Fn const kReal = reode::proxy::ResolveAs<Fn>("XInputGetCapabilities");
  return kReal ? kReal(user_index, flags, capabilities)
               : ERROR_DEVICE_NOT_CONNECTED;
}

void WINAPI ProxyXInputEnable(BOOL enable) {
  using Fn = void(WINAPI*)(BOOL);
  static Fn const kReal = reode::proxy::ResolveAs<Fn>("XInputEnable");
  if (kReal) kReal(enable);
}

DWORD WINAPI ProxyXInputGetBatteryInformation(DWORD user_index, BYTE dev_type,
                                              void* battery_information) {
  using Fn = DWORD(WINAPI*)(DWORD, BYTE, void*);
  static Fn const kReal =
      reode::proxy::ResolveAs<Fn>("XInputGetBatteryInformation");
  return kReal ? kReal(user_index, dev_type, battery_information)
               : ERROR_DEVICE_NOT_CONNECTED;
}

DWORD WINAPI ProxyXInputGetKeystroke(DWORD user_index, DWORD reserved,
                                     void* keystroke) {
  using Fn = DWORD(WINAPI*)(DWORD, DWORD, void*);
  static Fn const kReal = reode::proxy::ResolveAs<Fn>("XInputGetKeystroke");
  return kReal ? kReal(user_index, reserved, keystroke)
               : ERROR_DEVICE_NOT_CONNECTED;
}

DWORD WINAPI ProxyXInputGetAudioDeviceIds(DWORD user_index, LPWSTR render_id,
                                          UINT* render_count, LPWSTR capture_id,
                                          UINT* capture_count) {
  using Fn = DWORD(WINAPI*)(DWORD, LPWSTR, UINT*, LPWSTR, UINT*);
  static Fn const kReal =
      reode::proxy::ResolveAs<Fn>("XInputGetAudioDeviceIds");
  return kReal ? kReal(user_index, render_id, render_count, capture_id,
                       capture_count)
               : ERROR_DEVICE_NOT_CONNECTED;
}

// Undocumented ordinal-only exports. They take at most four pointer-sized
// arguments, so a uniform four-argument forwarder is ABI-compatible.
using OrdinalFn = DWORD(WINAPI*)(std::uintptr_t, std::uintptr_t, std::uintptr_t,
                                 std::uintptr_t);

#define REODE_FORWARD_ORDINAL(ordinal)                                         \
  DWORD WINAPI ProxyXInputOrdinal##ordinal(std::uintptr_t a, std::uintptr_t b, \
                                           std::uintptr_t c,                   \
                                           std::uintptr_t d) {                 \
    static OrdinalFn const kReal =                                             \
        reode::proxy::ResolveAs<OrdinalFn>(reode::proxy::Ordinal(ordinal));    \
    return kReal ? kReal(a, b, c, d) : ERROR_DEVICE_NOT_CONNECTED;             \
  }

REODE_FORWARD_ORDINAL(100)
REODE_FORWARD_ORDINAL(101)
REODE_FORWARD_ORDINAL(102)
REODE_FORWARD_ORDINAL(103)
REODE_FORWARD_ORDINAL(104)
REODE_FORWARD_ORDINAL(108)

#undef REODE_FORWARD_ORDINAL

}  // extern "C"

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID /*reserved*/) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(module);
    reode::proxy::InstallEntryHook(module);
  }
  return TRUE;
}
