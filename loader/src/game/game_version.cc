// Copyright 2026 The Reode Authors

#include "game/game_version.h"

#include <cstdint>
#include <string>

namespace reode::game {
namespace {

struct KnownBuild {
  std::uint32_t pe_timestamp;
  const char* version;
};

// PE timestamps of the Windows builds. A build that is not listed is
// reported as "unknown" and the user may set "game_version" in config.json.
constexpr KnownBuild kKnownBuilds[] = {
    {1731376950, "2.2074"}, {1768823161, "2.208"},  {1768959392, "2.2081"},
    {1791899187, "2.209"},  {1792203143, "2.2091"},
};

}  // namespace

std::string DetectGameVersion(std::uint32_t pe_timestamp,
                              const std::string& override_version) {
  if (!override_version.empty()) return override_version;
  for (const KnownBuild& build : kKnownBuilds) {
    if (build.version != nullptr && build.pe_timestamp == pe_timestamp) {
      return build.version;
    }
  }
  return "unknown";
}

}  // namespace reode::game
