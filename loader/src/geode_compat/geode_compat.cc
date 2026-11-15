// Copyright 2026 The Reode Authors

#include "geode_compat/geode_compat.h"

#include <windows.h>

#include <expected>
#include <filesystem>
#include <format>
#include <string>
#include <utility>

#include "core/logger.h"
#include "core/paths.h"
#include "geode_compat/pe_imports.h"
#include "mods/version.h"

namespace reode::geode_compat {
namespace {

constexpr char kGeodeModuleName[] = "Geode.dll";
constexpr int kSupportedGeodeMajor = 5;

template <typename Fn>
Fn ResolveShimFunction(HMODULE module, const char* name) {
  return reinterpret_cast<Fn>(GetProcAddress(module, name));
}

}  // namespace

GeodeCompat::GeodeCompat(std::filesystem::path shim_path)
    : shim_path_(std::move(shim_path)) {}

GeodeCompat::Assessment GeodeCompat::Assess(const mods::ModMetadata& metadata,
                                            const std::filesystem::path& binary,
                                            const std::string& game_version) {
  Assessment result;

  // Geode mods hard-code addresses of game functions for one specific game
  // build, so running them on any other build would crash.
  if (game_version == "unknown") {
    result.reason =
        "the game version is unknown; set \"game_version\" in "
        "reode/config.json to load Geode mods";
    return result;
  }
  if (!metadata.game_version.empty() && metadata.game_version != "*" &&
      metadata.game_version != game_version) {
    result.reason = std::format(
        "built for Geometry Dash {}, running {}. Wait for a version of the "
        "mod built for {}",
        metadata.game_version, game_version, game_version);
    return result;
  }

  // Geode keeps its ABI stable within a major version only.
  auto geode_version = mods::Version::Parse(metadata.loader_version);
  if (!geode_version || geode_version->major != kSupportedGeodeMajor) {
    result.reason =
        std::format("built for Geode {}, Reode implements the Geode {} ABI",
                    metadata.loader_version, REODE_GEODE_SHIM_SDK_VERSION);
    return result;
  }

  auto imports = ReadImportsFrom(binary, kGeodeModuleName);
  if (!imports) {
    result.reason =
        std::format("cannot read {}: {}", PathToUtf8(binary), imports.error());
    return result;
  }

  if (!imports->empty()) {
    if (!shim_exports_) {
      auto exports = ReadExportNames(shim_path_);
      if (!exports) {
        result.reason =
            std::format("the Geode compatibility layer ({}) is missing",
                        PathToUtf8(shim_path_));
        return result;
      }
      shim_exports_ = *std::move(exports);
    }
    for (const ImportedSymbol& symbol : *imports) {
      if (symbol.name.empty()) {
        result.missing_symbols.push_back(
            std::format("<ordinal {}>", symbol.ordinal));
      } else if (!shim_exports_->contains(symbol.name)) {
        result.missing_symbols.push_back(Undecorate(symbol.name));
      }
    }
  }

  if (!result.missing_symbols.empty()) {
    result.reason = std::format(
        "{} of {} Geode API functions it uses are not implemented by Reode "
        "yet",
        result.missing_symbols.size(), imports->size());
    return result;
  }
  result.loadable = true;
  return result;
}

std::expected<void, std::string> GeodeCompat::EnsureShimLoaded(
    const ReodeApi* api) {
  if (shim_module_ != nullptr) return {};
  // A module named Geode.dll that is already loaded would be the real
  // Geode, which must not run next to Reode.
  if (HMODULE existing = GetModuleHandleA(kGeodeModuleName);
      existing != nullptr) {
    return std::unexpected(
        "another Geode.dll is already loaded; uninstall Geode's loader first");
  }
  shim_module_ = LoadLibraryW(shim_path_.c_str());
  if (shim_module_ == nullptr) {
    return std::unexpected(std::format("cannot load {} (error {})",
                                       PathToUtf8(shim_path_), GetLastError()));
  }
  auto initialize = ResolveShimFunction<ReodeGeodeShimInitializeFn>(
      shim_module_, "ReodeGeodeShimInitialize");
  begin_mod_ = ResolveShimFunction<ReodeGeodeShimBeginModFn>(
      shim_module_, "ReodeGeodeShimBeginMod");
  finish_mod_ = ResolveShimFunction<ReodeGeodeShimFinishModFn>(
      shim_module_, "ReodeGeodeShimFinishMod");
  abort_mod_ = ResolveShimFunction<ReodeGeodeShimAbortModFn>(
      shim_module_, "ReodeGeodeShimAbortMod");
  if (initialize == nullptr || begin_mod_ == nullptr ||
      finish_mod_ == nullptr || abort_mod_ == nullptr) {
    return std::unexpected(std::format(
        "{} is not Reode's Geode compatibility layer", PathToUtf8(shim_path_)));
  }
  if (ReodeStatus status = initialize(api); status != kReodeOk) {
    return std::unexpected(
        std::format("the Geode compatibility layer failed to start (error {})",
                    static_cast<int>(status)));
  }
  LogInfo("Geode compatibility layer loaded from {}", PathToUtf8(shim_path_));
  return {};
}

std::expected<HMODULE, std::string> GeodeCompat::LoadMod(
    ReodeMod* reode_mod, const std::string& manifest,
    const std::filesystem::path& binary,
    const std::filesystem::path& resources_dir,
    const std::filesystem::path& save_dir) {
  ReodeGeodeMod* mod = begin_mod_(reode_mod, manifest.c_str(),
                                  PathToUtf8(resources_dir).c_str(),
                                  PathToUtf8(save_dir).c_str(), /*flags=*/0);
  if (mod == nullptr) {
    return std::unexpected("the compatibility layer rejected mod.json");
  }
  HMODULE module = LoadLibraryExW(
      binary.c_str(), nullptr,
      LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
  if (module == nullptr) {
    DWORD error = GetLastError();
    abort_mod_(mod);
    return std::unexpected(std::format("LoadLibrary failed (error {})", error));
  }
  if (ReodeStatus status = finish_mod_(mod, module); status != kReodeOk) {
    return std::unexpected(std::format(
        "the mod failed to initialize (error {})", static_cast<int>(status)));
  }
  return module;
}

}  // namespace reode::geode_compat
