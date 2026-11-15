// Copyright 2026 The Reode Authors
//
// geode::VersionInfo.
//
// Mods import even the inline members of this class from Geode.dll, so in
// this translation unit the class is declared dllexport, which makes the
// compiler emit and export every member.

#include "Geode/platform/platform.hpp"
#undef GEODE_DLL
#define GEODE_DLL __declspec(dllexport)
#include "Geode/utils/VersionInfo.hpp"
#undef GEODE_DLL
#define GEODE_DLL

#include <charconv>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "fmt/format.h"

namespace geode {
namespace {

std::optional<std::size_t> ParseNumber(std::string_view text) {
  std::size_t value = 0;
  auto [ptr, error] =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc() || ptr != text.data() + text.size() ||
      text.empty()) {
    return std::nullopt;
  }
  return value;
}

std::string TagToString(const VersionTag& tag) {
  std::string text;
  switch (tag.value) {
    case VersionTag::Alpha:
      text = "alpha";
      break;
    case VersionTag::Beta:
      text = "beta";
      break;
    case VersionTag::Prerelease:
      text = "prerelease";
      break;
  }
  if (tag.number) text += "." + std::to_string(*tag.number);
  return text;
}

std::optional<VersionTag> ParseTag(std::string_view text) {
  std::optional<std::size_t> number;
  if (std::size_t dot = text.find('.'); dot != std::string_view::npos) {
    number = ParseNumber(text.substr(dot + 1));
    if (!number) return std::nullopt;
    text = text.substr(0, dot);
  }
  if (text == "alpha") return VersionTag(VersionTag::Alpha, number);
  if (text == "beta") return VersionTag(VersionTag::Beta, number);
  if (text == "prerelease" || text == "pr") {
    return VersionTag(VersionTag::Prerelease, number);
  }
  return std::nullopt;
}

}  // namespace

Result<VersionInfo> VersionInfo::parse(std::string string) {
  std::string_view text = string;
  if (text.starts_with('v') || text.starts_with('V')) text.remove_prefix(1);
  std::optional<VersionTag> tag;
  if (std::size_t dash = text.find('-'); dash != std::string_view::npos) {
    tag = ParseTag(text.substr(dash + 1));
    if (!tag) {
      return Err(fmt::format("invalid version tag in \"{}\"", string));
    }
    text = text.substr(0, dash);
  }
  std::size_t parts[3] = {0, 0, 0};
  for (int i = 0; i < 3; ++i) {
    std::size_t dot = text.find('.');
    std::string_view part = text.substr(0, dot);
    auto number = ParseNumber(part);
    if (!number) return Err(fmt::format("invalid version \"{}\"", string));
    parts[i] = *number;
    if (dot == std::string_view::npos) {
      if (i != 2) return Err(fmt::format("invalid version \"{}\"", string));
      break;
    }
    if (i == 2) return Err(fmt::format("invalid version \"{}\"", string));
    text.remove_prefix(dot + 1);
  }
  return Ok(VersionInfo(parts[0], parts[1], parts[2], tag));
}

std::string VersionInfo::toNonVString(bool includeTag) const {
  std::string text = fmt::format("{}.{}.{}", m_major, m_minor, m_patch);
  if (includeTag && m_tag) text += "-" + TagToString(*m_tag);
  return text;
}

std::string VersionInfo::toVString(bool includeTag) const {
  return "v" + toNonVString(includeTag);
}

std::string format_as(VersionInfo const& version) {
  return version.toVString();
}

}  // namespace geode
