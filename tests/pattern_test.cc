// Copyright 2026 The Reode Authors

#include "game/pattern.h"

#include <cstdint>
#include <vector>

#include "gtest/gtest.h"

namespace reode::game {
namespace {

TEST(PatternTest, FindsPatternWithWildcards) {
  std::vector<std::uint8_t> code = {0x90, 0x48, 0x8B, 0xC4, 0x48, 0x89};
  auto pattern = Pattern::Parse("48 ?? C4 ? 89");
  ASSERT_TRUE(pattern.has_value());
  EXPECT_EQ(pattern->size(), 5u);
  EXPECT_EQ(pattern->FindIn(code), 1u);
}

TEST(PatternTest, ReportsMissingMatch) {
  std::vector<std::uint8_t> code = {0x90, 0x90};
  EXPECT_FALSE(Pattern::Parse("90 90 90")->FindIn(code).has_value());
}

TEST(PatternTest, RejectsMalformedPatterns) {
  EXPECT_FALSE(Pattern::Parse("").has_value());
  EXPECT_FALSE(Pattern::Parse("4").has_value());
  EXPECT_FALSE(Pattern::Parse("ZZ").has_value());
  EXPECT_FALSE(Pattern::Parse("123").has_value());
}

}  // namespace
}  // namespace reode::game
