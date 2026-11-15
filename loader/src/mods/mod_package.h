// Copyright 2026 The Reode Authors

#ifndef REODE_LOADER_SRC_MODS_MOD_PACKAGE_H_
#define REODE_LOADER_SRC_MODS_MOD_PACKAGE_H_

#include <expected>
#include <filesystem>
#include <string>

namespace reode::mods {

// Reads mod.json from a .reode / .geode package (a zip archive).
std::expected<std::string, std::string> ReadPackageManifest(
    const std::filesystem::path& package);

// Extracts `package` into `destination`, unless it was already extracted
// from the same package file (same size and modification time). Entries
// with absolute paths or ".." components are rejected.
std::expected<void, std::string> ExtractPackage(
    const std::filesystem::path& package,
    const std::filesystem::path& destination);

}  // namespace reode::mods

#endif  // REODE_LOADER_SRC_MODS_MOD_PACKAGE_H_
