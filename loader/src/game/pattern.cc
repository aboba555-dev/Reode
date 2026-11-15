// Copyright 2026 The Reode Authors

#include "game/pattern.h"

#include <charconv>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace reode::game {

std::optional<Pattern> Pattern::Parse(std::string_view text) {
  std::vector<std::optional<std::uint8_t>> bytes;
  std::size_t pos = 0;
  while (pos < text.size()) {
    if (text[pos] == ' ') {
      ++pos;
      continue;
    }
    std::size_t end = text.find(' ', pos);
    if (end == std::string_view::npos) end = text.size();
    std::string_view token = text.substr(pos, end - pos);
    if (token == "?" || token == "??") {
      bytes.emplace_back(std::nullopt);
    } else {
      if (token.size() != 2) return std::nullopt;
      std::uint8_t value = 0;
      auto [ptr, error] =
          std::from_chars(token.data(), token.data() + token.size(), value, 16);
      if (error != std::errc() || ptr != token.data() + token.size()) {
        return std::nullopt;
      }
      bytes.emplace_back(value);
    }
    pos = end;
  }
  if (bytes.empty()) return std::nullopt;
  return Pattern(std::move(bytes));
}

std::optional<std::size_t> Pattern::FindIn(
    std::span<const std::uint8_t> haystack) const {
  if (haystack.size() < bytes_.size()) return std::nullopt;
  const std::size_t last = haystack.size() - bytes_.size();
  for (std::size_t start = 0; start <= last; ++start) {
    bool matched = true;
    for (std::size_t i = 0; i < bytes_.size(); ++i) {
      if (bytes_[i] && *bytes_[i] != haystack[start + i]) {
        matched = false;
        break;
      }
    }
    if (matched) return start;
  }
  return std::nullopt;
}

}  // namespace reode::game
