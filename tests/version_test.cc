// Copyright 2026 The Reode Authors

#include "mods/version.h"

#include "gtest/gtest.h"

namespace reode::mods {
namespace {

Version V(const char* text) { return *Version::Parse(text); }

TEST(VersionTest, ParsesCommonForms) {
  EXPECT_EQ(V("1.2.3"), (Version{1, 2, 3, ""}));
  EXPECT_EQ(V("v1.2.3"), (Version{1, 2, 3, ""}));
  EXPECT_EQ(V("2.0"), (Version{2, 0, 0, ""}));
  EXPECT_EQ(V("1.0.0-beta.1"), (Version{1, 0, 0, "beta.1"}));
  EXPECT_FALSE(Version::Parse("").has_value());
  EXPECT_FALSE(Version::Parse("1..2").has_value());
  EXPECT_FALSE(Version::Parse("1.2.3.4").has_value());
  EXPECT_FALSE(Version::Parse("one").has_value());
}

TEST(VersionTest, OrdersPrereleasesBeforeReleases) {
  EXPECT_LT(V("1.0.0-alpha"), V("1.0.0"));
  EXPECT_LT(V("1.0.0"), V("1.0.1"));
  EXPECT_LT(V("1.9.0"), V("1.10.0"));
}

TEST(VersionRequirementTest, MatchesOperators) {
  auto at_least = *VersionRequirement::Parse(">=1.2.0");
  EXPECT_TRUE(at_least.IsSatisfiedBy(V("1.2.0")));
  EXPECT_TRUE(at_least.IsSatisfiedBy(V("3.0.0")));
  EXPECT_FALSE(at_least.IsSatisfiedBy(V("1.1.9")));

  auto compatible = *VersionRequirement::Parse("v1.2.0");
  EXPECT_TRUE(compatible.IsSatisfiedBy(V("1.5.0")));
  EXPECT_FALSE(compatible.IsSatisfiedBy(V("2.0.0")));

  auto exact = *VersionRequirement::Parse("=1.0.0");
  EXPECT_TRUE(exact.IsSatisfiedBy(V("1.0.0")));
  EXPECT_FALSE(exact.IsSatisfiedBy(V("1.0.1")));

  EXPECT_TRUE(VersionRequirement::Parse("*")->IsSatisfiedBy(V("9.9.9")));
  EXPECT_FALSE(VersionRequirement::Parse(">=x").has_value());
}

}  // namespace
}  // namespace reode::mods
