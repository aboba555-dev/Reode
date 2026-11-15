// Copyright 2026 The Reode Authors
//
// The stable, C-compatible binary interface between Reode.dll and mods.
//
// Mods never link against Reode.dll. Instead the loader hands every mod a
// pointer to a ReodeApi function table when it calls the mod's exported
// ReodeModLoad() function. New functions are only ever appended to the end of
// the table, and `size` tells a mod which of them exist, so mods built
// against an older SDK keep working with newer loaders.
//
// Most mods should use the C++ wrapper in <reode/reode.h> instead of calling
// this table directly.

#ifndef REODE_LOADER_INCLUDE_REODE_API_H_
#define REODE_LOADER_INCLUDE_REODE_API_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Bumped only on incompatible changes. Appending functions does not bump it.
#define REODE_API_VERSION 1

typedef struct ReodeMod ReodeMod;    // Opaque handle owned by the loader.
typedef struct ReodeHook ReodeHook;  // Opaque handle owned by the loader.

typedef enum ReodeStatus {
  kReodeOk = 0,
  kReodeInvalidArgument = 1,
  kReodeNotFound = 2,
  kReodeAlreadyExists = 3,
  kReodeUnsupportedInstruction = 4,
  kReodeOutOfMemory = 5,
  kReodeMemoryProtection = 6,
  kReodeIncompatible = 7,
  kReodeInternalError = 8,
} ReodeStatus;

typedef enum ReodeLogLevel {
  kReodeLogDebug = 0,
  kReodeLogInfo = 1,
  kReodeLogWarning = 2,
  kReodeLogError = 3,
} ReodeLogLevel;

typedef struct ReodeApi {
  uint32_t version;  // REODE_API_VERSION of the running loader.
  uint32_t size;     // sizeof(ReodeApi) of the running loader.

  // Writes a UTF-8 message to the debug console and the log file.
  void (*log)(ReodeMod* mod, ReodeLogLevel level, const char* message);

  // Installs an inline hook on `target`. `*original` receives a callable
  // pointer to the next function in the hook chain. Hooks with a higher
  // priority run first. The hook starts enabled.
  ReodeStatus (*create_hook)(ReodeMod* mod, void* target, void* detour,
                             int32_t priority, void** original,
                             ReodeHook** out_hook);
  ReodeStatus (*set_hook_enabled)(ReodeHook* hook, int32_t enabled);
  ReodeStatus (*remove_hook)(ReodeHook* hook);

  // Base address of GeometryDash.exe in memory.
  void* (*game_base)(void);
  // Version string of the running game, e.g. "2.2081", or "unknown".
  const char* (*game_version)(void);

  // Locates a vtable through the game's MSVC RTTI. `class_name` is either a
  // C++ name ("MenuLayer", "cocos2d::CCLayer") or a decorated one
  // (".?AVMenuLayer@@"). `subobject_offset` selects the base-class subobject
  // for classes with multiple inheritance; 0 is the primary vtable.
  void* (*find_vtable)(const char* class_name, uint32_t subobject_offset);
  // Returns entry `slot` of the primary vtable of `class_name`.
  void* (*find_virtual)(const char* class_name, uint32_t slot);
  // Scans the game's code for an IDA-style byte pattern ("48 8B ?? C4").
  void* (*find_pattern)(const char* pattern);

  // Information about the calling mod. Strings live as long as the mod.
  const char* (*mod_id)(ReodeMod* mod);
  const char* (*mod_version)(ReodeMod* mod);
  // UTF-8 path of a writable per-mod directory for settings and saves.
  const char* (*mod_save_dir)(ReodeMod* mod);
} ReodeApi;

// Signature of the function every Reode mod must export as "ReodeModLoad".
typedef ReodeStatus (*ReodeModLoadFn)(const ReodeApi* api, ReodeMod* mod);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // REODE_LOADER_INCLUDE_REODE_API_H_
