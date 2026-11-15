// Copyright 2026 The Reode Authors

#include "mods/mod_manager.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <map>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "core/logger.h"
#include "core/paths.h"
#include "game/cocos.h"
#include "geode_compat/geode_compat.h"
#include "mods/api_impl.h"
#include "mods/mod_package.h"

namespace reode::mods {
namespace {

constexpr char kReodeExtension[] = ".reode";
constexpr char kGeodeExtension[] = ".geode";
constexpr char kEntryPointName[] = "ReodeModLoad";
// Missing-symbol lines printed per incompatible Geode mod.
constexpr std::size_t kMaxMissingSymbolsLogged = 15;

std::string LowercaseExtension(const std::filesystem::path& path) {
  std::string extension = PathToUtf8(path.extension());
  std::ranges::transform(extension, extension.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return extension;
}

std::vector<std::filesystem::path> ListPackages(
    const std::filesystem::path& directory) {
  std::vector<std::filesystem::path> packages;
  std::error_code error;
  for (const auto& entry :
       std::filesystem::directory_iterator(directory, error)) {
    if (!entry.is_regular_file(error)) continue;
    std::string extension = LowercaseExtension(entry.path());
    if (extension == kReodeExtension || extension == kGeodeExtension) {
      packages.push_back(entry.path());
    }
  }
  std::ranges::sort(packages);
  return packages;
}

}  // namespace

ModManager& ModManager::Instance() {
  static ModManager* const kInstance = new ModManager();
  return *kInstance;
}

void ModManager::Fail(LoadedMod& mod, std::string error) {
  mod.state = ModState::kFailed;
  mod.error = std::move(error);
  LogError("[{}] {}", mod.metadata.id, mod.error);
}

LoadedMod* ModManager::Find(const std::string& id) {
  for (const auto& mod : mods_) {
    if (mod->metadata.id == id) return mod.get();
  }
  return nullptr;
}

void ModManager::AddPackage(const std::filesystem::path& package) {
  auto mod = std::make_unique<LoadedMod>();
  mod->package = package;
  mod->handle.mod = mod.get();

  auto manifest = ReadPackageManifest(package);
  if (!manifest) {
    mod->metadata.id = PathToUtf8(package.filename());
    Fail(*mod, manifest.error());
    mods_.push_back(std::move(mod));
    return;
  }
  auto metadata = ParseModMetadata(*manifest);
  if (!metadata) {
    mod->metadata.id = PathToUtf8(package.filename());
    Fail(*mod, metadata.error());
    mods_.push_back(std::move(mod));
    return;
  }
  if (Find(metadata->id) != nullptr) {
    LogWarning("Skipping {}: a mod with id {} was already found",
               PathToUtf8(package), metadata->id);
    return;
  }
  mod->metadata = *std::move(metadata);
  mod->manifest = *std::move(manifest);
  mod->version_text = mod->metadata.version.ToString();
  mods_.push_back(std::move(mod));
}

void ModManager::Discover(const Config& config) {
  const Paths& paths = GetPaths();
  for (const auto& package : ListPackages(paths.mods_dir)) {
    AddPackage(package);
  }
  if (config.load_geode_mods) {
    for (const auto& package : ListPackages(paths.geode_mods_dir)) {
      AddPackage(package);
    }
  }
  for (const auto& mod : mods_) {
    if (mod->state == ModState::kPending &&
        config.disabled_mods.contains(mod->metadata.id)) {
      mod->state = ModState::kDisabled;
      mod->error = "disabled in config.json";
      LogInfo("[{}] disabled in config.json", mod->metadata.id);
    }
  }
}

// Depth-first topological sort. `marks`: 1 = in progress, 2 = done.
bool ModManager::Visit(LoadedMod& mod, std::map<std::string, int>& marks,
                       std::vector<LoadedMod*>& order) {
  int& mark = marks[mod.metadata.id];
  if (mark == 2) return mod.state == ModState::kPending;
  if (mark == 1) {
    Fail(mod, "circular dependency");
    return false;
  }
  if (mod.state != ModState::kPending) {
    mark = 2;
    return false;
  }
  mark = 1;
  for (const Dependency& dependency : mod.metadata.dependencies) {
    LoadedMod* target = Find(dependency.id);
    std::string problem;
    if (target == nullptr) {
      problem = "is not installed";
    } else if (!dependency.requirement.IsSatisfiedBy(
                   target->metadata.version)) {
      problem = std::format("version {} does not satisfy {}",
                            target->metadata.version.ToString(),
                            dependency.requirement.ToString());
    } else if (!Visit(*target, marks, order)) {
      problem = "failed to load or is disabled";
    }
    if (!problem.empty()) {
      if (dependency.required) {
        marks[mod.metadata.id] = 2;
        Fail(mod, std::format("dependency {} {}", dependency.id, problem));
        return false;
      }
      LogWarning("[{}] optional dependency {} {}", mod.metadata.id,
                 dependency.id, problem);
    }
  }
  marks[mod.metadata.id] = 2;
  order.push_back(&mod);
  return true;
}

void ModManager::ResolveDependencies() {
  std::map<std::string, int> marks;
  for (const auto& mod : mods_) Visit(*mod, marks, load_order_);
}

void ModManager::LoadReodeMod(LoadedMod& mod,
                              const std::filesystem::path& binary) {
  const std::string expected_api = std::to_string(REODE_API_VERSION);
  if (mod.metadata.loader_version != expected_api) {
    Fail(mod, std::format("targets Reode API {}, this loader provides {}",
                          mod.metadata.loader_version, expected_api));
    return;
  }
  mod.module = LoadLibraryExW(
      binary.c_str(), nullptr,
      LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
  if (mod.module == nullptr) {
    Fail(mod, std::format("LoadLibrary failed (error {})", GetLastError()));
    return;
  }
  auto entry = reinterpret_cast<ReodeModLoadFn>(
      GetProcAddress(mod.module, kEntryPointName));
  if (entry == nullptr) {
    Fail(mod, std::format("binary does not export {}", kEntryPointName));
    return;
  }
  ReodeStatus status = entry(GetApiTable(), &mod.handle);
  if (status != kReodeOk) {
    Fail(mod, std::format("{} returned error {}", kEntryPointName,
                          static_cast<int>(status)));
    return;
  }
  mod.state = ModState::kLoaded;
}

void ModManager::LoadGeodeMod(LoadedMod& mod,
                              const std::filesystem::path& binary,
                              const std::string& game_version) {
#if !REODE_GEODE_COMPAT
  // Built without Geode.dll (REODE_GEODE_COMPAT=OFF, the default).
  (void)binary;
  (void)game_version;
  Fail(mod, "this build of Reode does not run Geode mods");
  return;
#else
  static geode_compat::GeodeCompat* const kCompat =
      new geode_compat::GeodeCompat(GetPaths().compat_dir / L"Geode.dll");
  auto assessment = kCompat->Assess(mod.metadata, binary, game_version);
  if (!assessment.loadable) {
    Fail(mod, "Geode mod cannot run yet: " + assessment.reason);
    for (std::size_t i = 0;
         i < assessment.missing_symbols.size() && i < kMaxMissingSymbolsLogged;
         ++i) {
      LogDebug("[{}]   missing: {}", mod.metadata.id,
               assessment.missing_symbols[i]);
    }
    return;
  }
  if (auto shim = kCompat->EnsureShimLoaded(GetApiTable()); !shim) {
    Fail(mod, shim.error());
    return;
  }
  auto module = kCompat->LoadMod(
      &mod.handle, mod.manifest, binary, mod.directory,
      GetPaths().saves_dir / mod.metadata.id);
  if (!module) {
    Fail(mod, module.error());
    return;
  }
  mod.module = *module;
  mod.state = ModState::kLoaded;
#endif
}

void ModManager::Load(LoadedMod& mod, const std::string& game_version) {
  const Paths& paths = GetPaths();
  mod.directory = paths.unzipped_dir / mod.metadata.id;
  if (auto extracted = ExtractPackage(mod.package, mod.directory); !extracted) {
    Fail(mod, "cannot extract package: " + extracted.error());
    return;
  }
  std::filesystem::path save_dir = paths.saves_dir / mod.metadata.id;
  std::error_code error;
  std::filesystem::create_directories(save_dir, error);
  mod.save_dir = PathToUtf8(save_dir);

  std::filesystem::path binary = mod.directory / mod.metadata.binary;
  if (!std::filesystem::exists(binary, error)) {
    Fail(mod, std::format("package does not contain {}", mod.metadata.binary));
    return;
  }

  if (mod.metadata.format == ModFormat::kReode) {
    LoadReodeMod(mod, binary);
  } else {
    LoadGeodeMod(mod, binary, game_version);
  }
  if (mod.state == ModState::kLoaded) {
    LogInfo("Loaded {} {} ({})", mod.metadata.id, mod.version_text,
            mod.metadata.format == ModFormat::kReode ? "Reode" : "Geode");
  }
}

void ModManager::LoadResources() {
  const cocos::Cocos* cocos = cocos::Cocos::Get();
  if (cocos == nullptr) return;
  for (const auto& mod : mods_) {
    if (mod->state != ModState::kLoaded) continue;
    std::filesystem::path resources = mod->directory / L"resources";
    std::error_code error;
    if (!std::filesystem::is_directory(resources, error)) continue;
    if (!search_paths_added_) {
      cocos->AddSearchPath(PathToUtf8(resources).c_str());
    }
    for (const std::string& sheet : mod->metadata.spritesheets) {
      // cocos2d picks the -hd / -uhd variants for the texture quality. The
      // texture is passed explicitly: the packaged plists name it relative
      // to the resources root, but cocos2d resolves that name relative to
      // the plist's own directory.
      std::string base = mod->metadata.id + "/" + sheet;
      if (!search_paths_added_ &&
          !cocos->LoadTexture((base + ".png").c_str())) {
        LogWarning("[{}] texture {}.png of spritesheet not found",
                   mod->metadata.id, base);
        continue;
      }
      cocos->AddSpriteFrames((base + ".plist").c_str(),
                             (base + ".png").c_str());
      if (!search_paths_added_) {
        LogDebug("[{}] spritesheet {} loaded", mod->metadata.id, sheet);
      }
    }
  }
  if (!search_paths_added_) {
    search_paths_added_ = true;
    LogDebug("Mod resources added to the cocos2d search paths");
  }
}

void ModManager::LoadAll(const Config& config,
                         const std::string& game_version) {
  config_ = config;
  Discover(config);
  if (!safe_mode_reason_.empty()) {
    for (const auto& mod : mods_) {
      if (mod->state != ModState::kPending) continue;
      mod->state = ModState::kDisabled;
      mod->error = "safe mode";
    }
    LogWarning("Safe mode: no mods are loaded ({})", safe_mode_reason_);
    return;
  }
  ResolveDependencies();
  for (LoadedMod* mod : load_order_) {
    // A dependency may have failed while loading after ordering.
    bool dependencies_ok = std::ranges::all_of(
        mod->metadata.dependencies, [&](const Dependency& dependency) {
          LoadedMod* target = Find(dependency.id);
          return !dependency.required ||
                 (target != nullptr && target->state == ModState::kLoaded);
        });
    if (!dependencies_ok) {
      Fail(*mod, "a required dependency failed to load");
      continue;
    }
    Load(*mod, game_version);
  }
  std::size_t loaded = std::ranges::count_if(
      mods_, [](const auto& mod) { return mod->state == ModState::kLoaded; });
  LogInfo("{} of {} mods loaded", loaded, mods_.size());
}

}  // namespace reode::mods
