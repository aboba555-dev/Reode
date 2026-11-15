// Copyright 2026 The Reode Authors

#ifndef REODE_LOADER_SRC_GAME_GAME_VERSION_H_
#define REODE_LOADER_SRC_GAME_GAME_VERSION_H_

#include <cstdint>
#include <string>

namespace reode::game {

// Identifies the running Geometry Dash build. The executable carries no
// version resource, so builds are recognised by their PE timestamp.
// `override_version` (from config.json) wins when non-empty.
std::string DetectGameVersion(std::uint32_t pe_timestamp,
                              const std::string& override_version);

}  // namespace reode::game

#endif  // REODE_LOADER_SRC_GAME_GAME_VERSION_H_
