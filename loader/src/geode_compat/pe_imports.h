// Copyright 2026 The Reode Authors

#ifndef REODE_LOADER_SRC_GEODE_COMPAT_PE_IMPORTS_H_
#define REODE_LOADER_SRC_GEODE_COMPAT_PE_IMPORTS_H_

#include <expected>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace reode::geode_compat {

// One imported symbol. Ordinal-only imports have an empty name.
struct ImportedSymbol {
  std::string name;
  int ordinal = -1;
};

// Lists the symbols `dll` imports from the module named `from_module`
// (case-insensitive). The DLL is mapped as an image resource, so none of its
// code runs.
std::expected<std::vector<ImportedSymbol>, std::string> ReadImportsFrom(
    const std::filesystem::path& dll, std::string_view from_module);

// Lists the names exported by `dll`, again without running it.
std::expected<std::set<std::string>, std::string> ReadExportNames(
    const std::filesystem::path& dll);

// Turns a decorated MSVC name into a readable signature, best effort.
std::string Undecorate(const std::string& decorated);

}  // namespace reode::geode_compat

#endif  // REODE_LOADER_SRC_GEODE_COMPAT_PE_IMPORTS_H_
