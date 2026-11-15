// Copyright 2026 The Reode Authors

#ifndef REODE_LOADER_SRC_GAME_RTTI_H_
#define REODE_LOADER_SRC_GAME_RTTI_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "game/module_image.h"

namespace reode::game {

// Converts "cocos2d::CCLayer" into the MSVC type descriptor name
// ".?AVCCLayer@cocos2d@@". Decorated names (starting with ".?A") are
// returned unchanged. `is_struct` selects the ".?AU" (struct) form.
std::string DecorateTypeName(std::string_view name, bool is_struct);

// Finds a vtable through the MSVC RTTI emitted into `image`:
//
//   TypeDescriptor (in .data)  { vftable*, spare, ".?AVName@@" }
//   CompleteObjectLocator      { signature = 1, offset, cd_offset,
//                                type_rva, hierarchy_rva, self_rva }
//   vtable[-1]                 -> CompleteObjectLocator
//
// `subobject_offset` is the COL's `offset` field: 0 for the primary vtable,
// the base-class offset for secondary vtables. Returns 0 if not found.
// Results are cached.
std::uintptr_t FindVTable(const ModuleImage& image, std::string_view name,
                          std::uint32_t subobject_offset);

// Returns entry `slot` of the primary vtable of `name`, or 0.
std::uintptr_t FindVirtualFunction(const ModuleImage& image,
                                   std::string_view name, std::uint32_t slot);

}  // namespace reode::game

#endif  // REODE_LOADER_SRC_GAME_RTTI_H_
