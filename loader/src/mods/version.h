// Copyright 2026 The Reode Authors

#ifndef REODE_LOADER_SRC_MODS_VERSION_H_
#define REODE_LOADER_SRC_MODS_VERSION_H_

#include <compare>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace reode::mods {

// A semantic version "major.minor.patch[-tag]". A leading "v" (as used by
// Geode mods) is accepted. Missing minor / patch components default to 0.
struct Version {
  int major = 0;
  int minor = 0;
  int patch = 0;
  std::string tag;  // Pre-release tag without the dash, e.g. "beta.1".

  static std::optional<Version> Parse(std::string_view text);
  std::string ToString() const;

  // Pre-release versions order before the corresponding release.
  std::strong_ordering operator<=>(const Version& other) const;
  bool operator==(const Version& other) const = default;
};

// A constraint on a dependency version: ">=1.2.0", "<2.0.0", "=1.0.0",
// "*" (anything), or a bare "1.2.0", which means ">=1.2.0" within the same
// major version.
class VersionRequirement {
 public:
  enum class Comparison {
    kAny,
    kCompatible,
    kEqual,
    kGreater,
    kGreaterOrEqual,
    kLess,
    kLessOrEqual,
  };

  VersionRequirement() = default;
  VersionRequirement(Comparison comparison, Version version)
      : comparison_(comparison), version_(std::move(version)) {}

  static std::optional<VersionRequirement> Parse(std::string_view text);

  bool IsSatisfiedBy(const Version& version) const;
  std::string ToString() const;

 private:
  Comparison comparison_ = Comparison::kAny;
  Version version_;
};

}  // namespace reode::mods

#endif  // REODE_LOADER_SRC_MODS_VERSION_H_
