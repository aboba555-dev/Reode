// Copyright 2026 The Reode Authors

#include "game/rtti.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace reode::game {
namespace {

struct CompleteObjectLocator {
  std::uint32_t signature;
  std::uint32_t offset;
  std::uint32_t constructor_displacement_offset;
  std::uint32_t type_descriptor_rva;
  std::uint32_t class_hierarchy_rva;
  std::uint32_t self_rva;
};

// Offset of the name inside a TypeDescriptor: vftable pointer + spare.
constexpr std::uint32_t kTypeDescriptorNameOffset = 16;

std::vector<std::uintptr_t> FindAll(ModuleImage::Section section,
                                    std::span<const std::uint8_t> needle,
                                    std::size_t alignment) {
  std::vector<std::uintptr_t> matches;
  auto haystack = section.bytes();
  std::boyer_moore_horspool_searcher searcher(needle.begin(), needle.end());
  auto it = haystack.begin();
  while (true) {
    auto [first, last] = searcher(it, haystack.end());
    if (first == haystack.end()) break;
    auto address = section.begin + (first - haystack.begin());
    if (address % alignment == 0) matches.push_back(address);
    it = first + 1;
  }
  return matches;
}

template <typename T>
std::span<const std::uint8_t> AsBytes(const T& value) {
  return {reinterpret_cast<const std::uint8_t*>(&value), sizeof(T)};
}

std::uintptr_t FindVTableUncached(const ModuleImage& image,
                                  const std::string& decorated,
                                  std::uint32_t subobject_offset) {
  auto data = image.FindSection(".data");
  auto rdata = image.FindSection(".rdata");
  if (!data || !rdata) return 0;

  std::span<const std::uint8_t> name(
      reinterpret_cast<const std::uint8_t*>(decorated.c_str()),
      decorated.size() + 1);  // Include the terminator to avoid prefixes.
  for (std::uintptr_t name_address : FindAll(*data, name, 1)) {
    std::uintptr_t descriptor = name_address - kTypeDescriptorNameOffset;
    auto descriptor_rva = static_cast<std::uint32_t>(descriptor - image.base());

    for (std::uintptr_t field : FindAll(*rdata, AsBytes(descriptor_rva), 4)) {
      std::uintptr_t col_address =
          field - offsetof(CompleteObjectLocator, type_descriptor_rva);
      if (!rdata->Contains(col_address, sizeof(CompleteObjectLocator))) {
        continue;
      }
      CompleteObjectLocator col{};
      std::memcpy(&col, reinterpret_cast<const void*>(col_address),
                  sizeof(col));
      if (col.signature != 1 || col.offset != subobject_offset ||
          image.base() + col.self_rva != col_address) {
        continue;
      }
      std::uint64_t col_pointer = col_address;
      auto references = FindAll(*rdata, AsBytes(col_pointer), 8);
      if (!references.empty()) return references.front() + sizeof(void*);
    }
  }
  return 0;
}

class VTableCache {
 public:
  static VTableCache& Instance() {
    static VTableCache* const kCache = new VTableCache();
    return *kCache;
  }

  std::uintptr_t Get(const ModuleImage& image, const std::string& decorated,
                     std::uint32_t subobject_offset) {
    std::lock_guard lock(mutex_);
    auto key = std::make_tuple(image.base(), decorated, subobject_offset);
    if (auto it = cache_.find(key); it != cache_.end()) return it->second;
    std::uintptr_t vtable =
        FindVTableUncached(image, decorated, subobject_offset);
    cache_.emplace(std::move(key), vtable);
    return vtable;
  }

 private:
  std::mutex mutex_;
  std::map<std::tuple<std::uintptr_t, std::string, std::uint32_t>,
           std::uintptr_t>
      cache_;
};

}  // namespace

std::string DecorateTypeName(std::string_view name, bool is_struct) {
  if (name.starts_with(".?A")) return std::string(name);
  // Split "a::b::C" into components and emit them innermost first.
  std::vector<std::string_view> parts;
  std::size_t start = 0;
  while (true) {
    std::size_t separator = name.find("::", start);
    parts.push_back(name.substr(start, separator - start));
    if (separator == std::string_view::npos) break;
    start = separator + 2;
  }
  std::string result = is_struct ? ".?AU" : ".?AV";
  for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
    result.append(*it);
    result.push_back('@');
  }
  result.push_back('@');
  return result;
}

std::uintptr_t FindVTable(const ModuleImage& image, std::string_view name,
                          std::uint32_t subobject_offset) {
  for (bool is_struct : {false, true}) {
    std::uintptr_t vtable = VTableCache::Instance().Get(
        image, DecorateTypeName(name, is_struct), subobject_offset);
    if (vtable != 0) return vtable;
    if (name.starts_with(".?A")) break;
  }
  return 0;
}

std::uintptr_t FindVirtualFunction(const ModuleImage& image,
                                   std::string_view name, std::uint32_t slot) {
  std::uintptr_t vtable = FindVTable(image, name, 0);
  if (vtable == 0) return 0;
  auto rdata = image.FindSection(".rdata");
  auto text = image.FindSection(".text");
  std::uintptr_t entry_address = vtable + slot * sizeof(void*);
  if (!rdata || !text || !rdata->Contains(entry_address, sizeof(void*))) {
    return 0;
  }
  std::uintptr_t function =
      *reinterpret_cast<const std::uintptr_t*>(entry_address);
  return text->Contains(function) ? function : 0;
}

}  // namespace reode::game
