// Copyright 2026 The Reode Authors

#include "proxy/entry_hook.h"

#include <windows.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <string>

namespace reode::proxy {
namespace {

// mov rax, imm64; jmp rax
constexpr std::size_t kPatchSize = 12;

using EntryPoint = int (*)();
using InitializeFn = void (*)(HMODULE proxy_module);

constinit HMODULE g_proxy_module = nullptr;
constinit EntryPoint g_original_entry = nullptr;
constinit std::array<std::uint8_t, kPatchSize> g_saved_bytes{};

bool WriteCode(void* address, const void* bytes, std::size_t size) {
  DWORD old_protect = 0;
  if (!VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &old_protect)) {
    return false;
  }
  std::memcpy(address, bytes, size);
  VirtualProtect(address, size, old_protect, &old_protect);
  FlushInstructionCache(GetCurrentProcess(), address, size);
  return true;
}

std::wstring GameDirectory() {
  std::wstring path(MAX_PATH, L'\0');
  DWORD length =
      GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
  path.resize(length);
  std::size_t slash = path.find_last_of(L"\\/");
  return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

bool IsGeometryDash() {
  std::wstring path(MAX_PATH, L'\0');
  DWORD length =
      GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
  path.resize(length);
  std::size_t slash = path.find_last_of(L"\\/");
  std::wstring name = path.substr(slash == std::wstring::npos ? 0 : slash + 1);
  return CompareStringOrdinal(name.c_str(), -1, L"GeometryDash.exe", -1,
                              TRUE) == CSTR_EQUAL;
}

void LoadReode() {
  std::wstring dll_path = GameDirectory() + L"\\Reode.dll";
  HMODULE reode = LoadLibraryW(dll_path.c_str());
  if (reode == nullptr) {
    MessageBoxW(nullptr,
                L"Reode.dll could not be loaded. The game will start "
                L"without mods.",
                L"Reode", MB_ICONWARNING | MB_OK);
    return;
  }
  auto initialize =
      reinterpret_cast<InitializeFn>(GetProcAddress(reode, "ReodeInitialize"));
  if (initialize == nullptr) {
    MessageBoxW(nullptr, L"Reode.dll does not export ReodeInitialize.",
                L"Reode", MB_ICONERROR | MB_OK);
    return;
  }
  initialize(g_proxy_module);
}

// Runs in place of the executable's entry point. Restores the original
// bytes, brings Reode up and then continues into the real entry point.
int EntryHook() {
  WriteCode(reinterpret_cast<void*>(g_original_entry), g_saved_bytes.data(),
            g_saved_bytes.size());
  LoadReode();
  return g_original_entry();
}

}  // namespace

void InstallEntryHook(HMODULE proxy_module) {
  if (!IsGeometryDash()) return;
  g_proxy_module = proxy_module;

  auto* base = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
  auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
  auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
  std::uint8_t* entry = base + nt->OptionalHeader.AddressOfEntryPoint;
  g_original_entry = reinterpret_cast<EntryPoint>(entry);
  std::memcpy(g_saved_bytes.data(), entry, kPatchSize);

  std::array<std::uint8_t, kPatchSize> patch = {0x48, 0xB8};  // mov rax, imm
  auto target = reinterpret_cast<std::uint64_t>(&EntryHook);
  std::memcpy(patch.data() + 2, &target, sizeof(target));
  patch[10] = 0xFF;  // jmp rax
  patch[11] = 0xE0;
  WriteCode(entry, patch.data(), patch.size());
}

}  // namespace reode::proxy
