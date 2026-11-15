// Copyright 2026 The Reode Authors

#ifndef REODE_GEODE_SHIM_SRC_HOST_H_
#define REODE_GEODE_SHIM_SRC_HOST_H_

#include <string_view>

#include "reode/api.h"

namespace reode::geode_shim {

// Stores the Reode API table received in ReodeGeodeShimInitialize().
void SetApi(const ReodeApi* api);

// The Reode API table. Only valid after initialization.
const ReodeApi& Api();
bool HasApi();

// Logs through Reode. `mod` may be null for messages from the shim itself.
void Log(ReodeMod* mod, ReodeLogLevel level, std::string_view message);

}  // namespace reode::geode_shim

#endif  // REODE_GEODE_SHIM_SRC_HOST_H_
