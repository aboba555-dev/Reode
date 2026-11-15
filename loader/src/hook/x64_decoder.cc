// Copyright 2026 The Reode Authors

#include "hook/x64_decoder.h"

#include <cstdint>
#include <cstring>
#include <optional>
#include <span>

namespace reode::hook {
namespace {

constexpr int kMaxInstructionLength = 15;

enum class OpcodeMap { kOneByte, k0F, k0F38, k0F3A, kOther };

// Opcodes that do not exist in 64-bit mode.
bool IsInvalidIn64BitMode(std::uint8_t op) {
  switch (op) {
    case 0x06:
    case 0x07:
    case 0x0E:
    case 0x16:
    case 0x17:
    case 0x1E:
    case 0x1F:
    case 0x27:
    case 0x2F:
    case 0x37:
    case 0x3F:
    case 0x60:
    case 0x61:
    case 0x82:
    case 0x9A:
    case 0xCE:
    case 0xD4:
    case 0xD5:
    case 0xD6:
    case 0xEA:
      return true;
    default:
      return false;
  }
}

bool OneByteHasModRm(std::uint8_t op) {
  if (op < 0x40) return (op & 0x07) < 0x04;
  if (op >= 0x80 && op <= 0x8F) return true;
  if (op >= 0xD0 && op <= 0xD3) return true;
  if (op >= 0xD8 && op <= 0xDF) return true;
  switch (op) {
    case 0x63:
    case 0x69:
    case 0x6B:
    case 0xC0:
    case 0xC1:
    case 0xC6:
    case 0xC7:
    case 0xF6:
    case 0xF7:
    case 0xFE:
    case 0xFF:
      return true;
    default:
      return false;
  }
}

bool TwoByteHasModRm(std::uint8_t op) {
  if (op >= 0x80 && op <= 0x8F) return false;  // jcc rel32
  if (op >= 0xC8 && op <= 0xCF) return false;  // bswap
  switch (op) {
    case 0x05:
    case 0x06:
    case 0x07:
    case 0x08:
    case 0x09:
    case 0x0B:
    case 0x0E:
    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33:
    case 0x34:
    case 0x35:
    case 0x37:
    case 0x77:
    case 0xA0:
    case 0xA1:
    case 0xA2:
    case 0xA8:
    case 0xA9:
    case 0xAA:
      return false;
    default:
      return true;
  }
}

bool TwoByteHasImm8(std::uint8_t op) {
  switch (op) {
    case 0x0F:
    case 0x70:
    case 0x71:
    case 0x72:
    case 0x73:
    case 0xA4:
    case 0xAC:
    case 0xBA:
    case 0xC2:
    case 0xC4:
    case 0xC5:
    case 0xC6:
      return true;
    default:
      return false;
  }
}

std::int64_t ReadSigned(std::span<const std::uint8_t> code, int offset,
                        int size) {
  if (size == 1) return static_cast<std::int8_t>(code[offset]);
  std::int32_t value = 0;
  std::memcpy(&value, code.data() + offset, sizeof(value));
  return value;
}

}  // namespace

std::optional<DecodedInstruction> DecodeInstruction(
    std::span<const std::uint8_t> code, std::uint64_t address) {
  DecodedInstruction result;
  const int available = static_cast<int>(code.size() < kMaxInstructionLength
                                             ? code.size()
                                             : kMaxInstructionLength);
  int pos = 0;
  auto has = [&](int count) { return pos + count <= available; };

  bool operand_size_16 = false;
  bool address_size_32 = false;
  bool rex_w = false;

  // Legacy prefixes.
  for (;;) {
    if (!has(1)) return std::nullopt;
    std::uint8_t byte = code[pos];
    if (byte == 0x66) {
      operand_size_16 = true;
    } else if (byte == 0x67) {
      address_size_32 = true;
    } else if (byte != 0xF0 && byte != 0xF2 && byte != 0xF3 && byte != 0x2E &&
               byte != 0x36 && byte != 0x3E && byte != 0x26 && byte != 0x64 &&
               byte != 0x65) {
      break;
    }
    ++pos;
  }

  // REX prefix (must directly precede the opcode).
  if (has(1) && (code[pos] & 0xF0) == 0x40) {
    rex_w = (code[pos] & 0x08) != 0;
    ++pos;
  }

  if (!has(1)) return std::nullopt;
  std::uint8_t opcode = code[pos++];
  OpcodeMap map = OpcodeMap::kOneByte;
  bool has_modrm = false;
  int immediate_size = 0;
  // Operand-size dependent immediate (imm16 / imm32).
  const int imm_z = operand_size_16 ? 2 : 4;

  if (opcode == 0xC4 || opcode == 0xC5 || opcode == 0x62) {
    // VEX (C4/C5) and EVEX (62) are always prefixes in 64-bit mode.
    int prefix_bytes = opcode == 0xC5 ? 1 : (opcode == 0xC4 ? 2 : 3);
    if (!has(prefix_bytes + 1)) return std::nullopt;
    int map_select = 1;
    if (opcode == 0xC4) map_select = code[pos] & 0x1F;
    if (opcode == 0x62) map_select = code[pos] & 0x07;
    pos += prefix_bytes;
    std::uint8_t vex_opcode = code[pos++];
    switch (map_select) {
      case 1:
        map = OpcodeMap::k0F;
        break;
      case 2:
        map = OpcodeMap::k0F38;
        break;
      case 3:
        map = OpcodeMap::k0F3A;
        break;
      case 5:
      case 6:
        map = OpcodeMap::kOther;  // EVEX FP16 maps: ModRM, no immediate.
        break;
      default:
        return std::nullopt;
    }
    // vzeroupper / vzeroall have no ModRM byte.
    has_modrm =
        !(opcode != 0x62 && map == OpcodeMap::k0F && vex_opcode == 0x77);
    if (map == OpcodeMap::k0F3A ||
        (map == OpcodeMap::k0F && TwoByteHasImm8(vex_opcode))) {
      immediate_size = 1;
    }
  } else if (opcode == 0x0F) {
    if (!has(1)) return std::nullopt;
    std::uint8_t second = code[pos++];
    if (second == 0x38) {
      if (!has(1)) return std::nullopt;
      ++pos;
      map = OpcodeMap::k0F38;
      has_modrm = true;
    } else if (second == 0x3A) {
      if (!has(1)) return std::nullopt;
      ++pos;
      map = OpcodeMap::k0F3A;
      has_modrm = true;
      immediate_size = 1;
    } else {
      map = OpcodeMap::k0F;
      has_modrm = TwoByteHasModRm(second);
      if (TwoByteHasImm8(second)) immediate_size = 1;
      if (second >= 0x80 && second <= 0x8F) {
        result.branch = BranchKind::kConditional;
        result.condition = second - 0x80;
        result.relative_size = 4;
      }
    }
  } else {
    if (IsInvalidIn64BitMode(opcode)) return std::nullopt;
    has_modrm = OneByteHasModRm(opcode);
    if (opcode < 0x40) {
      if ((opcode & 0x07) == 0x04) immediate_size = 1;
      if ((opcode & 0x07) == 0x05) immediate_size = imm_z;
    } else if (opcode >= 0x70 && opcode <= 0x7F) {
      result.branch = BranchKind::kConditional;
      result.condition = opcode - 0x70;
      result.relative_size = 1;
    } else if (opcode >= 0xB0 && opcode <= 0xB7) {
      immediate_size = 1;
    } else if (opcode >= 0xB8 && opcode <= 0xBF) {
      immediate_size = rex_w ? 8 : imm_z;
    } else if (opcode >= 0xA0 && opcode <= 0xA3) {
      immediate_size = address_size_32 ? 4 : 8;  // moffs
    } else if (opcode >= 0xE0 && opcode <= 0xE3) {
      result.branch = BranchKind::kLoop;
      result.relative_size = 1;
    } else {
      switch (opcode) {
        case 0x6A:
        case 0x6B:
        case 0x80:
        case 0x83:
        case 0xA8:
        case 0xC0:
        case 0xC1:
        case 0xC6:
        case 0xCD:
        case 0xE4:
        case 0xE5:
        case 0xE6:
        case 0xE7:
          immediate_size = 1;
          break;
        case 0x68:
        case 0x69:
        case 0x81:
        case 0xA9:
        case 0xC7:
          immediate_size = imm_z;
          break;
        case 0xC2:
        case 0xCA:
          immediate_size = 2;
          break;
        case 0xC8:
          immediate_size = 3;
          break;
        case 0xE8:
          result.branch = BranchKind::kCall;
          result.relative_size = 4;
          break;
        case 0xE9:
          result.branch = BranchKind::kJump;
          result.relative_size = 4;
          break;
        case 0xEB:
          result.branch = BranchKind::kJump;
          result.relative_size = 1;
          break;
        default:
          break;
      }
    }
  }

  if (has_modrm) {
    if (!has(1)) return std::nullopt;
    std::uint8_t modrm = code[pos++];
    int mod = modrm >> 6;
    int reg = (modrm >> 3) & 0x07;
    int rm = modrm & 0x07;

    // test r/m, imm (F6 /0, /1 and F7 /0, /1) carries an immediate.
    if (map == OpcodeMap::kOneByte && (opcode == 0xF6 || opcode == 0xF7) &&
        reg < 2) {
      immediate_size = opcode == 0xF6 ? 1 : imm_z;
    }

    if (mod != 3) {
      int displacement_size = 0;
      if (rm == 4) {
        if (!has(1)) return std::nullopt;
        std::uint8_t sib = code[pos++];
        if ((sib & 0x07) == 5 && mod == 0) displacement_size = 4;
      }
      if (mod == 0 && rm == 5) {
        result.rip_relative = true;
        result.displacement_offset = pos;
        displacement_size = 4;
      } else if (mod == 1) {
        displacement_size = 1;
      } else if (mod == 2) {
        displacement_size = 4;
      }
      if (!has(displacement_size)) return std::nullopt;
      pos += displacement_size;
    }
  }

  if (result.relative_size != 0) {
    if (!has(result.relative_size)) return std::nullopt;
    result.relative_offset = pos;
    pos += result.relative_size;
  }
  if (!has(immediate_size)) return std::nullopt;
  pos += immediate_size;

  result.length = pos;
  if (result.branch != BranchKind::kNone) {
    result.branch_target =
        address + pos +
        ReadSigned(code, result.relative_offset, result.relative_size);
  }
  if (result.rip_relative) {
    // The displacement is relative to the end of the whole instruction.
    result.rip_target =
        address + pos + ReadSigned(code, result.displacement_offset, 4);
  }
  return result;
}

}  // namespace reode::hook
