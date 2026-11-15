// Copyright 2026 The Reode Authors

#include "game/rtti.h"

#include <windows.h>

#include <cstdint>

#include "gtest/gtest.h"

namespace reode::game::testing {

// A polymorphic class compiled into this test binary, so the RTTI lookup can
// be checked against the vtable the compiler itself produced.
class RttiProbe {
 public:
  virtual ~RttiProbe() = default;
  virtual int Value() const { return 42; }
};

namespace {

TEST(RttiTest, DecoratesTypeNames) {
  EXPECT_EQ(DecorateTypeName("MenuLayer", false), ".?AVMenuLayer@@");
  EXPECT_EQ(DecorateTypeName("cocos2d::CCLayer", false),
            ".?AVCCLayer@cocos2d@@");
  EXPECT_EQ(DecorateTypeName("a::b::C", true), ".?AUC@b@a@@");
  EXPECT_EQ(DecorateTypeName(".?AVAlready@@", false), ".?AVAlready@@");
}

TEST(RttiTest, FindsVTableOfLocalClass) {
  RttiProbe* volatile probe = new RttiProbe();
  auto expected_vtable = *reinterpret_cast<std::uintptr_t*>(probe);
  ModuleImage self(GetModuleHandleW(nullptr));
  std::uintptr_t vtable =
      FindVTable(self, "reode::game::testing::RttiProbe", 0);
  EXPECT_EQ(vtable, expected_vtable);
  EXPECT_EQ(FindVirtualFunction(self, "reode::game::testing::RttiProbe", 1),
            reinterpret_cast<const std::uintptr_t*>(expected_vtable)[1]);
  EXPECT_EQ(FindVTable(self, "NoSuchClassAnywhere", 0), 0u);
  delete probe;
}

}  // namespace
}  // namespace reode::game::testing
