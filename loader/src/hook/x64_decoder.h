// Copyright 2026 The Reode Authors

#ifndef REODE_LOADER_SRC_HOOK_X64_DECODER_H_
#define REODE_LOADER_SRC_HOOK_X64_DECODER_H_

#include <cstdint>
#include <optional>
#include <span>

namespace reode::hook {

// Kind of control transfer encoded by an instruction with a relative target.
enum class BranchKind {
  kNone,
  kJump,         // EB rel8, E9 rel32
  kConditional,  // 70-7F rel8, 0F 80-8F rel32
  kCall,         // E8 rel32
  kLoop,         // E0-E3 rel8 (loop/jrcxz), cannot be relocated
};

// Length and relocation information for one x86-64 instruction.
struct DecodedInstruction {
  int length = 0;

  // True for [rip + disp32] memory operands.
  bool rip_relative = false;
  // Offset of the disp32 field inside the instruction when rip_relative.
  int displacement_offset = 0;

  BranchKind branch = BranchKind::kNone;
  // Offset and size (1 or 4) of the relative branch immediate.
  int relative_offset = 0;
  int relative_size = 0;
  // Condition code 0-15 for kConditional branches.
  int condition = 0;

  // Absolute branch target, valid when branch != kNone. Computed from the
  // `address` passed to DecodeInstruction.
  std::uint64_t branch_target = 0;
  // Absolute address referenced by a rip-relative operand.
  std::uint64_t rip_target = 0;
};

// Decodes the instruction at the start of `code`, which is located at
// `address` in memory. Returns std::nullopt for invalid or truncated input.
//
// This is a length decoder, not a full disassembler: it understands every
// encoding needed to relocate function prologues (legacy, REX, 0F/0F38/0F3A
// maps, VEX and EVEX) but does not identify mnemonics.
std::optional<DecodedInstruction> DecodeInstruction(
    std::span<const std::uint8_t> code, std::uint64_t address);

}  // namespace reode::hook

#endif  // REODE_LOADER_SRC_HOOK_X64_DECODER_H_
