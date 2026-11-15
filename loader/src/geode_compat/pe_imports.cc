// Copyright 2026 The Reode Authors

#include "geode_compat/pe_imports.h"

#include <windows.h>
// dbghelp.h must come after windows.h.
#include <dbghelp.h>

#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace reode::geode_compat {
namespace {

// Maps a DLL with image layout (so RVAs can be used directly) without
// executing DllMain or resolving its imports.
class MappedImage {
 public:
  explicit MappedImage(const std::filesystem::path& path)
      : module_(LoadLibraryExW(
            path.c_str(), nullptr,
            LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE)) {}
  ~MappedImage() {
    if (module_ != nullptr) FreeLibrary(module_);
  }
  MappedImage(const MappedImage&) = delete;
  MappedImage& operator=(const MappedImage&) = delete;

  bool ok() const { return module_ != nullptr; }

  // Data-file handles have their low bits tagged; strip them for the base.
  std::uintptr_t base() const {
    return reinterpret_cast<std::uintptr_t>(module_) & ~std::uintptr_t{3};
  }

  const IMAGE_DATA_DIRECTORY* Directory(int index) const {
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
    const auto* nt =
        reinterpret_cast<const IMAGE_NT_HEADERS64*>(base() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
      return nullptr;
    }
    const IMAGE_DATA_DIRECTORY* directory =
        &nt->OptionalHeader.DataDirectory[index];
    return directory->VirtualAddress == 0 ? nullptr : directory;
  }

  template <typename T>
  const T* At(std::uint32_t rva) const {
    return reinterpret_cast<const T*>(base() + rva);
  }

 private:
  HMODULE module_;
};

}  // namespace

std::expected<std::vector<ImportedSymbol>, std::string> ReadImportsFrom(
    const std::filesystem::path& dll, std::string_view from_module) {
  MappedImage image(dll);
  if (!image.ok()) return std::unexpected("cannot map DLL");
  std::vector<ImportedSymbol> symbols;
  const IMAGE_DATA_DIRECTORY* directory =
      image.Directory(IMAGE_DIRECTORY_ENTRY_IMPORT);
  if (directory == nullptr) return symbols;

  for (const auto* descriptor =
           image.At<IMAGE_IMPORT_DESCRIPTOR>(directory->VirtualAddress);
       descriptor->Name != 0; ++descriptor) {
    std::string_view name = image.At<char>(descriptor->Name);
    if (CompareStringOrdinal(
            std::wstring(name.begin(), name.end()).c_str(), -1,
            std::wstring(from_module.begin(), from_module.end()).c_str(), -1,
            TRUE) != CSTR_EQUAL) {
      continue;
    }
    std::uint32_t thunk_rva = descriptor->OriginalFirstThunk != 0
                                  ? descriptor->OriginalFirstThunk
                                  : descriptor->FirstThunk;
    for (const auto* thunk = image.At<IMAGE_THUNK_DATA64>(thunk_rva);
         thunk->u1.AddressOfData != 0; ++thunk) {
      if (IMAGE_SNAP_BY_ORDINAL64(thunk->u1.Ordinal)) {
        symbols.push_back(
            {"", static_cast<int>(IMAGE_ORDINAL64(thunk->u1.Ordinal))});
      } else {
        const auto* by_name = image.At<IMAGE_IMPORT_BY_NAME>(
            static_cast<std::uint32_t>(thunk->u1.AddressOfData));
        symbols.push_back({by_name->Name, -1});
      }
    }
  }
  return symbols;
}

std::expected<std::set<std::string>, std::string> ReadExportNames(
    const std::filesystem::path& dll) {
  MappedImage image(dll);
  if (!image.ok()) return std::unexpected("cannot map DLL");
  std::set<std::string> names;
  const IMAGE_DATA_DIRECTORY* directory =
      image.Directory(IMAGE_DIRECTORY_ENTRY_EXPORT);
  if (directory == nullptr) return names;
  const auto* exports =
      image.At<IMAGE_EXPORT_DIRECTORY>(directory->VirtualAddress);
  const auto* name_rvas = image.At<std::uint32_t>(exports->AddressOfNames);
  for (DWORD i = 0; i < exports->NumberOfNames; ++i) {
    names.insert(image.At<char>(name_rvas[i]));
  }
  return names;
}

std::string Undecorate(const std::string& decorated) {
  std::array<char, 1024> buffer{};
  DWORD length =
      UnDecorateSymbolName(decorated.c_str(), buffer.data(),
                           static_cast<DWORD>(buffer.size()), UNDNAME_COMPLETE);
  if (length == 0) return decorated;
  return std::string(buffer.data(), length);
}

}  // namespace reode::geode_compat
