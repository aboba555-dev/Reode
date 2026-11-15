// Copyright 2026 The Reode Authors

#ifndef REODE_PROXY_ENTRY_HOOK_H_
#define REODE_PROXY_ENTRY_HOOK_H_

#include <windows.h>

namespace reode::proxy {

// Redirects the game executable's entry point so that Reode.dll is loaded on
// the main thread, outside of the loader lock, right before the game's own
// CRT starts. Does nothing if the host process is not Geometry Dash.
void InstallEntryHook(HMODULE proxy_module);

}  // namespace reode::proxy

#endif  // REODE_PROXY_ENTRY_HOOK_H_
