// Copyright 2026 The Reode Authors

#ifndef REODE_LOADER_SRC_MODS_API_IMPL_H_
#define REODE_LOADER_SRC_MODS_API_IMPL_H_

#include <string>

#include "reode/api.h"

namespace reode::mods {

// Stores the detected game version returned through ReodeApi::game_version.
void SetGameVersionForApi(std::string version);

// The function table handed to every Reode mod.
const ReodeApi* GetApiTable();

}  // namespace reode::mods

#endif  // REODE_LOADER_SRC_MODS_API_IMPL_H_
