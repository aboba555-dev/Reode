// Copyright 2026 The Reode Authors
//
// C interface between Reode.dll and Reode's Geode.dll (the Geode
// compatibility layer). Reode.dll drives the life cycle of every Geode mod:
//
//   ReodeGeodeShimInitialize(api)            once, after loading Geode.dll
//   mod = ReodeGeodeShimBeginMod(...)        queue the mod for Mod::get()
//   LoadLibrary(<mod>.dll)                   runs the mod's static init
//   ReodeGeodeShimFinishMod(mod, module)     entry point, hooks, events
//
// ReodeGeodeShimAbortMod() replaces FinishMod when LoadLibrary fails.

#ifndef REODE_GEODE_SHIM_INCLUDE_REODE_GEODE_SHIM_HOST_H_
#define REODE_GEODE_SHIM_INCLUDE_REODE_GEODE_SHIM_HOST_H_

#include "reode/api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ReodeGeodeMod ReodeGeodeMod;  // Opaque, owned by the shim.

// Version of the Geode SDK whose ABI the shim implements.
#define REODE_GEODE_SHIM_SDK_VERSION "5.11.2"

typedef ReodeStatus (*ReodeGeodeShimInitializeFn)(const ReodeApi* api);

// `mod_json` is the UTF-8 text of the mod's mod.json. Directories are UTF-8.
// `flags` is reserved and must be 0.
typedef ReodeGeodeMod* (*ReodeGeodeShimBeginModFn)(ReodeMod* reode_mod,
                                                   const char* mod_json,
                                                   const char* resources_dir,
                                                   const char* save_dir,
                                                   uint32_t flags);

// `module` is the HMODULE returned by LoadLibrary.
typedef ReodeStatus (*ReodeGeodeShimFinishModFn)(ReodeGeodeMod* mod,
                                                 void* module);

typedef void (*ReodeGeodeShimAbortModFn)(ReodeGeodeMod* mod);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // REODE_GEODE_SHIM_INCLUDE_REODE_GEODE_SHIM_HOST_H_
