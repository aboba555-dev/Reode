// Copyright 2026 The Reode Authors

#include "mods/version.h"

#include <charconv>
#include <compare>
#include <format>
#include <optional>
#include <string>
#include <string_view>

namespace reode::mods {
namespace {

std::string_view Trim(std::string_view text) {
  while (!text.empty() && text.front() == ' ') text.remove_prefix(1);
  while (!text.empty() && text.back() == ' ') text.remove_suffix(1);
  return text;
}

bool ParseComponent(std::string_view text, int* out) {
  if (text.empty()) return false;
  auto [ptr, error] =
      std::from_chars(text.data(), text.data() + text.size(), *out);
  return error == std::errc() && ptr == text.data() + text.size() && *out >= 0;
}

}  // namespace

std::optional<Version> Version::Parse(std::string_view text) {
  text = Trim(text);
  if (text.starts_with('v') || text.starts_with('V')) text.remove_prefix(1);
  Version version;
  if (std::size_t dash = text.find('-'); dash != std::string_view::npos) {
    version.tag = std::string(text.substr(dash + 1));
    if (version.tag.empty()) return std::nullopt;
    text = text.substr(0, dash);
  }
  int* components[] = {&version.major, &version.minor, &version.patch};
  int index = 0;
  while (true) {
    if (index == 3) return std::nullopt;
    std::size_t dot = text.find('.');
    if (!ParseComponent(text.substr(0, dot), components[index++])) {
      return std::nullopt;
    }
    if (dot == std::string_view::npos) break;
    text.remove_prefix(dot + 1);
  }
  return version;
}

std::string Version::ToString() const {
  std::string result = std::format("{}.{}.{}", major, minor, patch);
  if (!tag.empty()) result += "-" + tag;
  return result;
}

std::strong_ordering Version::operator<=>(const Version& other) const {
  if (auto c = major <=> other.major; c != 0) return c;
  if (auto c = minor <=> other.minor; c != 0) return c;
  if (auto c = patch <=> other.patch; c != 0) return c;
  if (tag.empty() != other.tag.empty()) {
    return tag.empty() ? std::strong_ordering::greater
                       : std::strong_ordering::less;
  }
  return tag <=> other.tag;
}

std::optional<VersionRequirement> VersionRequirement::Parse(
    std::string_view text) {
  text = Trim(text);
  if (text.empty() || text == "*") return VersionRequirement();
  struct Prefix {
    std::string_view text;
    Comparison comparison;
  };
  // Longer operators first so ">=" is not read as ">".
  static constexpr Prefix kPrefixes[] = {
      {">=", Comparison::kGreaterOrEqual},
      {"<=", Comparison::kLessOrEqual},
      {"==", Comparison::kEqual},
      {">", Comparison::kGreater},
      {"<", Comparison::kLess},
      {"=", Comparison::kEqual},
      {"^", Comparison::kCompatible},
  };
  Comparison comparison = Comparison::kCompatible;
  for (const Prefix& prefix : kPrefixes) {
    if (text.starts_with(prefix.text)) {
      comparison = prefix.comparison;
      text.remove_prefix(prefix.text.size());
      break;
    }
  }
  auto version = Version::Parse(text);
  if (!version) return std::nullopt;
  return VersionRequirement(comparison, *std::move(version));
}

bool VersionRequirement::IsSatisfiedBy(const Version& version) const {
  switch (comparison_) {
    case Comparison::kAny:
      return true;
    case Comparison::kCompatible:
      return version.major == version_.major && version >= version_;
    case Comparison::kEqual:
      return version == version_;
    case Comparison::kGreater:
      return version > version_;
    case Comparison::kGreaterOrEqual:
      return version >= version_;
    case Comparison::kLess:
      return version < version_;
    case Comparison::kLessOrEqual:
      return version <= version_;
  }
  return false;
}

std::string VersionRequirement::ToString() const {
  switch (comparison_) {
    case Comparison::kAny:
      return "*";
    case Comparison::kCompatible:
      return "^" + version_.ToString();
    case Comparison::kEqual:
      return "=" + version_.ToString();
    case Comparison::kGreater:
      return ">" + version_.ToString();
    case Comparison::kGreaterOrEqual:
      return ">=" + version_.ToString();
    case Comparison::kLess:
      return "<" + version_.ToString();
    case Comparison::kLessOrEqual:
      return "<=" + version_.ToString();
  }
  return "?";
}

}  // namespace reode::mods
