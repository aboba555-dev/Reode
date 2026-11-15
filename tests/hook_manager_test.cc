// Copyright 2026 The Reode Authors

#include "hook/hook_manager.h"

#include <intrin.h>

#include "gtest/gtest.h"

namespace reode::hook {
namespace {

// Targets must be real, non-inlined functions large enough to patch.
__declspec(noinline) int AddTen(int value) {
  volatile int result = value;
  result = result + 10;
  return result;
}

__declspec(noinline) int Multiply(int a, int b) {
  volatile int result = a;
  result = result * b;
  return result;
}

using AddTenFn = int (*)(int);
using MultiplyFn = int (*)(int, int);

// Calls through a volatile pointer so the compiler cannot fold the call.
int CallAddTen(int value) {
  AddTenFn volatile function = &AddTen;
  return function(value);
}

int CallMultiply(int a, int b) {
  MultiplyFn volatile function = &Multiply;
  return function(a, b);
}

constinit AddTenFn g_add_original_a = nullptr;
constinit AddTenFn g_add_original_b = nullptr;
constinit MultiplyFn g_multiply_original = nullptr;

int AddDetourA(int value) { return g_add_original_a(value) * 2; }
int AddDetourB(int value) { return g_add_original_b(value + 1); }
int MultiplyDetour(int a, int b) { return g_multiply_original(a, b) + 1; }

TEST(HookManagerTest, ChainsHooksByPriorityAndToggles) {
  HookManager& manager = HookManager::Instance();
  ASSERT_EQ(CallAddTen(1), 11);

  auto hook_a = manager.Create(
      reinterpret_cast<void*>(&AddTen), reinterpret_cast<void*>(&AddDetourA),
      /*priority=*/0, "test", reinterpret_cast<void**>(&g_add_original_a));
  ASSERT_TRUE(hook_a.has_value());
  EXPECT_EQ(CallAddTen(1), 22);  // (1 + 10) * 2

  // Higher priority runs first: B(1) -> A(2) -> original(2) = 12 -> 24.
  auto hook_b = manager.Create(
      reinterpret_cast<void*>(&AddTen), reinterpret_cast<void*>(&AddDetourB),
      /*priority=*/10, "test", reinterpret_cast<void**>(&g_add_original_b));
  ASSERT_TRUE(hook_b.has_value());
  EXPECT_EQ(CallAddTen(1), 24);

  EXPECT_EQ(manager.SetEnabled(*hook_a, false), kReodeOk);
  EXPECT_EQ(CallAddTen(1), 12);  // Only B: original(2)

  EXPECT_EQ(manager.Remove(*hook_b), kReodeOk);
  EXPECT_EQ(CallAddTen(1), 11);  // Nothing enabled.

  EXPECT_EQ(manager.SetEnabled(*hook_a, true), kReodeOk);
  EXPECT_EQ(CallAddTen(1), 22);
  EXPECT_EQ(manager.Remove(*hook_a), kReodeOk);
  EXPECT_EQ(CallAddTen(1), 11);
}

TEST(HookManagerTest, RejectsDuplicateDetour) {
  HookManager& manager = HookManager::Instance();
  auto first =
      manager.Create(reinterpret_cast<void*>(&Multiply),
                     reinterpret_cast<void*>(&MultiplyDetour), 0, "test",
                     reinterpret_cast<void**>(&g_multiply_original));
  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(CallMultiply(3, 4), 13);

  MultiplyFn unused = nullptr;
  auto second = manager.Create(reinterpret_cast<void*>(&Multiply),
                               reinterpret_cast<void*>(&MultiplyDetour), 0,
                               "test", reinterpret_cast<void**>(&unused));
  ASSERT_FALSE(second.has_value());
  EXPECT_EQ(second.error(), kReodeAlreadyExists);
  EXPECT_EQ(manager.Remove(*first), kReodeOk);
}

TEST(HookManagerTest, RejectsNullArguments) {
  void* original = nullptr;
  auto hook =
      HookManager::Instance().Create(nullptr, nullptr, 0, "test", &original);
  ASSERT_FALSE(hook.has_value());
  EXPECT_EQ(hook.error(), kReodeInvalidArgument);
}

}  // namespace
}  // namespace reode::hook
