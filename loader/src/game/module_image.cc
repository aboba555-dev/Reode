// Copyright 2026 The Reode Authors

#include "game/module_image.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>

namespace reode::game {

ModuleImage::ModuleImage(void* base)
    : base_(reinterpret_cast<std::uintptr_t>(base)) {
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base_);
  const auto* nt =
      reinterpret_cast<const IMAGE_NT_HEADERS64*>(base_ + dos->e_lfanew);
  timestamp_ = nt->FileHeader.TimeDateStamp;
  image_size_ = nt->OptionalHeader.SizeOfImage;
}

const ModuleImage& ModuleImage::Game() {
  static const ModuleImage* const kGame =
      new ModuleImage(GetModuleHandleW(nullptr));
  return *kGame;
}

std::optional<ModuleImage::Section> ModuleImage::FindSection(
    std::string_view name) const {
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base_);
  const auto* nt =
      reinterpret_cast<const IMAGE_NT_HEADERS64*>(base_ + dos->e_lfanew);
  const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
  for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
    char section_name[IMAGE_SIZEOF_SHORT_NAME + 1] = {};
    std::memcpy(section_name, section->Name, IMAGE_SIZEOF_SHORT_NAME);
    if (name == section_name) {
      std::uintptr_t begin = base_ + section->VirtualAddress;
      return Section{begin, begin + section->Misc.VirtualSize};
    }
  }
  return std::nullopt;
}

}  // namespace reode::game
