// Copyright 2026 The Reode Authors
//
// Force-included before every shim source file (/FI).
//
// The Geode SDK marks its classes GEODE_DLL, which is either dllimport (in
// mods) or dllexport (in Geode's own loader). Neither works for Reode:
// dllimport forbids defining the functions, and dllexport would make every
// translation unit emit and export all inline members of every SDK class,
// pulling in Geode's entire UI and cocos2d. The shim therefore compiles the
// SDK classes as plain classes and exports exactly the symbols it
// implements, listed in geode_shim/geode.def.
//
// It then includes geode_internals.h, which has to come before any other
// Geode header. Files that need a different setup define
// REODE_GEODE_NO_INTERNALS (see geode_shim/CMakeLists.txt).

#ifndef REODE_GEODE_SHIM_SRC_GEODE_ABI_PRELUDE_H_
#define REODE_GEODE_SHIM_SRC_GEODE_ABI_PRELUDE_H_

#include "Geode/platform/platform.hpp"

#undef GEODE_DLL
#define GEODE_DLL

#ifndef REODE_GEODE_NO_INTERNALS
#include "geode_internals.h"
#endif

#endif  // REODE_GEODE_SHIM_SRC_GEODE_ABI_PRELUDE_H_
