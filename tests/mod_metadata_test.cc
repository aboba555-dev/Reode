// Copyright 2026 The Reode Authors

#include "mods/mod_metadata.h"

#include "gtest/gtest.h"

namespace reode::mods {
namespace {

TEST(ModMetadataTest, ParsesReodeManifest) {
  auto metadata = ParseModMetadata(R"({
    "reode": "1",
    "id": "author.cool_mod",
    "name": "Cool Mod",
    "version": "1.2.0",
    "developers": ["Author"],
    "gd": "2.2081",
    "dependencies": [{"id": "other.lib", "version": ">=1.0.0"}]
  })");
  ASSERT_TRUE(metadata.has_value()) << metadata.error();
  EXPECT_EQ(metadata->format, ModFormat::kReode);
  EXPECT_EQ(metadata->loader_version, "1");
  EXPECT_EQ(metadata->id, "author.cool_mod");
  EXPECT_EQ(metadata->version.ToString(), "1.2.0");
  EXPECT_EQ(metadata->game_version, "2.2081");
  EXPECT_EQ(metadata->binary, "author.cool_mod.dll");
  ASSERT_EQ(metadata->dependencies.size(), 1u);
  EXPECT_EQ(metadata->dependencies[0].id, "other.lib");
  EXPECT_TRUE(metadata->dependencies[0].required);
}

TEST(ModMetadataTest, ParsesGeodeManifestWithObjectDependencies) {
  auto metadata = ParseModMetadata(R"({
    "geode": "4.0.0",
    "gd": {"win": "2.2074", "android": "2.2074"},
    "id": "dev.geode-mod",
    "name": "Geode Mod",
    "version": "v2.1.0",
    "developer": "Dev",
    "dependencies": {
      "geode.node-ids": ">=1.12.0",
      "dev.optional": {"version": "1.0.0", "importance": "suggested"}
    }
  })");
  ASSERT_TRUE(metadata.has_value()) << metadata.error();
  EXPECT_EQ(metadata->format, ModFormat::kGeode);
  EXPECT_EQ(metadata->game_version, "2.2074");
  EXPECT_EQ(metadata->version.ToString(), "2.1.0");
  ASSERT_EQ(metadata->developers.size(), 1u);
  ASSERT_EQ(metadata->dependencies.size(), 2u);
  for (const Dependency& dependency : metadata->dependencies) {
    EXPECT_EQ(dependency.required, dependency.id == "geode.node-ids")
        << dependency.id;
  }
}

TEST(ModMetadataTest, RejectsUnsafeValues) {
  EXPECT_FALSE(ParseModMetadata(R"({"reode": "1", "id": "../evil",
                                    "version": "1.0.0"})")
                   .has_value());
  EXPECT_FALSE(ParseModMetadata(R"({"reode": "1", "id": "a.b",
                                    "version": "1.0.0",
                                    "binary": "..\\x.dll"})")
                   .has_value());
  EXPECT_FALSE(
      ParseModMetadata(R"({"id": "a.b", "version": "1.0.0"})").has_value());
  EXPECT_FALSE(ParseModMetadata("not json").has_value());
}

TEST(ModMetadataTest, ValidatesIds) {
  EXPECT_TRUE(IsValidModId("geode.node-ids"));
  EXPECT_TRUE(IsValidModId("a_b.c"));
  EXPECT_FALSE(IsValidModId(""));
  EXPECT_FALSE(IsValidModId("Upper.case"));
  EXPECT_FALSE(IsValidModId("a/b"));
  EXPECT_FALSE(IsValidModId("a..b"));
}

}  // namespace
}  // namespace reode::mods
