// Copyright 2026 The Reode Authors
//
// Includes the SDK headers whose loader-only members (ModMetadata setters,
// Mod::setMetadata, ...) Geode.dll defines and mods import. The SDK only
// declares them under GEODE_EXPOSE_SECRET_INTERNALS_IN_HEADERS_DO_NOT_DEFINE_
// PLEASE, and DefaultInclude.hpp rejects that macro for non-Clang compilers,
// so DefaultInclude.hpp is included first, without it.
//
// Must be the first Geode include of every file that needs these members:
// the headers use #pragma once, so a plain earlier include hides them.

#ifndef REODE_GEODE_SHIM_SRC_GEODE_INTERNALS_H_
#define REODE_GEODE_SHIM_SRC_GEODE_INTERNALS_H_

#include "Geode/DefaultInclude.hpp"
#define GEODE_EXPOSE_SECRET_INTERNALS_IN_HEADERS_DO_NOT_DEFINE_PLEASE
#include "Geode/loader/Mod.hpp"
#include "Geode/loader/ModMetadata.hpp"
#undef GEODE_EXPOSE_SECRET_INTERNALS_IN_HEADERS_DO_NOT_DEFINE_PLEASE

#endif  // REODE_GEODE_SHIM_SRC_GEODE_INTERNALS_H_
