// Copyright 2026 The Reode Authors

#ifndef REODE_LOADER_SRC_GAME_PATTERN_H_
#define REODE_LOADER_SRC_GAME_PATTERN_H_

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace reode::game {

// An IDA-style byte pattern such as "48 8B ?? C4 ? 90". A byte of
// `std::nullopt` is a wildcard.
class Pattern {
 public:
  // Returns std::nullopt if `text` is malformed or empty.
  static std::optional<Pattern> Parse(std::string_view text);

  // Returns the offset of the first match in `haystack`, if any.
  std::optional<std::size_t> FindIn(
      std::span<const std::uint8_t> haystack) const;

  std::size_t size() const { return bytes_.size(); }

 private:
  explicit Pattern(std::vector<std::optional<std::uint8_t>> bytes)
      : bytes_(std::move(bytes)) {}

  std::vector<std::optional<std::uint8_t>> bytes_;
};

}  // namespace reode::game

#endif  // REODE_LOADER_SRC_GAME_PATTERN_H_
