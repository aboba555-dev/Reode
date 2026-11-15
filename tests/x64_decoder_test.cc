// Copyright 2026 The Reode Authors

#include "hook/x64_decoder.h"

#include <cstdint>
#include <vector>

#include "gtest/gtest.h"

namespace reode::hook {
namespace {

constexpr std::uint64_t kAddress = 0x140001000;

int LengthOf(std::vector<std::uint8_t> bytes) {
  auto decoded = DecodeInstruction(bytes, kAddress);
  return decoded ? decoded->length : -1;
}

TEST(X64DecoderTest, DecodesMenuLayerInitPrologue) {
  // The real prologue of MenuLayer::init in the current game build.
  EXPECT_EQ(LengthOf({0x48, 0x8B, 0xC4}), 3);        // mov rax, rsp
  EXPECT_EQ(LengthOf({0x48, 0x89, 0x58, 0x08}), 4);  // mov [rax+8], rbx
  EXPECT_EQ(LengthOf({0x55}), 1);                    // push rbp
  EXPECT_EQ(LengthOf({0x41, 0x54}), 2);              // push r12
  EXPECT_EQ(LengthOf({0x48, 0x8D, 0x68, 0xA1}), 4);  // lea rbp, [rax-5f]
  EXPECT_EQ(LengthOf({0x48, 0x81, 0xEC, 0xF0, 0, 0, 0}), 7);  // sub rsp, 0xf0
  EXPECT_EQ(LengthOf({0x0F, 0x29, 0x70, 0xB8}), 4);  // movaps [rax-48], xmm6
}

TEST(X64DecoderTest, DecodesImmediates) {
  EXPECT_EQ(LengthOf({0x48, 0xB8, 1, 2, 3, 4, 5, 6, 7, 8}), 10);  // mov r64
  EXPECT_EQ(LengthOf({0xB8, 1, 2, 3, 4}), 5);                     // mov r32
  EXPECT_EQ(LengthOf({0x66, 0xB8, 1, 2}), 4);                     // mov r16
  EXPECT_EQ(LengthOf({0x48, 0x83, 0xEC, 0x28}), 4);               // sub rsp,28
  EXPECT_EQ(LengthOf({0xC7, 0x44, 0x24, 0x10, 1, 0, 0, 0}), 8);   // mov m32,i
  EXPECT_EQ(LengthOf({0xF6, 0x41, 0x10, 0x01}), 4);  // test byte [rcx+10],1
  EXPECT_EQ(LengthOf({0xF7, 0xD8}), 2);              // neg eax (no imm)
  EXPECT_EQ(LengthOf({0xC2, 0x08, 0x00}), 3);        // ret 8
}

TEST(X64DecoderTest, DecodesVexAndSse) {
  EXPECT_EQ(LengthOf({0xC5, 0xF8, 0x77}), 3);                    // vzeroupper
  EXPECT_EQ(LengthOf({0xC5, 0xFA, 0x10, 0x05, 0, 0, 0, 0}), 8);  // vmovss rip
  EXPECT_EQ(LengthOf({0x66, 0x0F, 0x3A, 0x0B, 0xC1, 0x04}), 6);  // roundsd
  EXPECT_EQ(LengthOf({0xF3, 0x0F, 0x10, 0x44, 0x24, 0x30}), 6);  // movss
}

TEST(X64DecoderTest, ReportsRipRelativeOperand) {
  // lea rcx, [rip + 0x100]
  std::vector<std::uint8_t> bytes = {0x48, 0x8D, 0x0D, 0x00, 0x01, 0, 0};
  auto decoded = DecodeInstruction(bytes, kAddress);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->length, 7);
  EXPECT_TRUE(decoded->rip_relative);
  EXPECT_EQ(decoded->displacement_offset, 3);
  EXPECT_EQ(decoded->rip_target, kAddress + 7 + 0x100);
}

TEST(X64DecoderTest, ReportsBranches) {
  std::vector<std::uint8_t> call = {0xE8, 0x10, 0x00, 0x00, 0x00};
  auto decoded = DecodeInstruction(call, kAddress);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->branch, BranchKind::kCall);
  EXPECT_EQ(decoded->branch_target, kAddress + 5 + 0x10);

  std::vector<std::uint8_t> short_jz = {0x74, 0xFE};  // jz $
  decoded = DecodeInstruction(short_jz, kAddress);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->branch, BranchKind::kConditional);
  EXPECT_EQ(decoded->condition, 4);
  EXPECT_EQ(decoded->branch_target, kAddress);

  std::vector<std::uint8_t> near_jne = {0x0F, 0x85, 0x00, 0x01, 0x00, 0x00};
  decoded = DecodeInstruction(near_jne, kAddress);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->length, 6);
  EXPECT_EQ(decoded->condition, 5);
  EXPECT_EQ(decoded->branch_target, kAddress + 6 + 0x100);
}

TEST(X64DecoderTest, RejectsInvalidAndTruncatedInput) {
  EXPECT_EQ(LengthOf({0x06}), -1);              // push es: invalid in x64
  EXPECT_EQ(LengthOf({0x48, 0x8B}), -1);        // missing ModRM
  EXPECT_EQ(LengthOf({0xE8, 0x00, 0x00}), -1);  // truncated rel32
  EXPECT_EQ(LengthOf({}), -1);
}

}  // namespace
}  // namespace reode::hook
