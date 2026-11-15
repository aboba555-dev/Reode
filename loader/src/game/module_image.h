// Copyright 2026 The Reode Authors

#ifndef REODE_LOADER_SRC_GAME_MODULE_IMAGE_H_
#define REODE_LOADER_SRC_GAME_MODULE_IMAGE_H_

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace reode::game {

// A view over a PE image mapped into the current process.
class ModuleImage {
 public:
  struct Section {
    std::uintptr_t begin = 0;
    std::uintptr_t end = 0;

    bool Contains(std::uintptr_t address, std::size_t size = 1) const {
      return address >= begin && address + size <= end;
    }
    std::span<const std::uint8_t> bytes() const {
      return {reinterpret_cast<const std::uint8_t*>(begin), end - begin};
    }
  };

  explicit ModuleImage(void* base);

  // The image of GeometryDash.exe.
  static const ModuleImage& Game();

  std::uintptr_t base() const { return base_; }
  std::uint32_t timestamp() const { return timestamp_; }
  std::uint32_t image_size() const { return image_size_; }

  std::optional<Section> FindSection(std::string_view name) const;

  // Resolves a virtual address from a relative one.
  template <typename T = void>
  T* At(std::uint32_t rva) const {
    return reinterpret_cast<T*>(base_ + rva);
  }

 private:
  std::uintptr_t base_ = 0;
  std::uint32_t timestamp_ = 0;
  std::uint32_t image_size_ = 0;
};

}  // namespace reode::game

#endif  // REODE_LOADER_SRC_GAME_MODULE_IMAGE_H_
