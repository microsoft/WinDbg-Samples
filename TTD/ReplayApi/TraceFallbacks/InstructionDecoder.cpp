// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License. See LICENSE in the project root for license information.

// InstructionDecoder.cpp - A utility class for decoding CPU instructions from their byte representation.

#include "InstructionDecoder.h"

// Make the sample code easier to read by bringing the TTD namespaces into scope.
using namespace TTD;
using namespace TTD::Replay;

// Initialize the instruction decoder for the specified architecture.
InstructionDecoder::InstructionDecoder(ProcessorArchitecture guestArchitecture)
    : m_guestArchitecture(guestArchitecture)
{
    switch (m_guestArchitecture)
    {
    case ProcessorArchitecture::x86:
        ZydisDecoderInit(&m_zydisDecoder, ZYDIS_MACHINE_MODE_LEGACY_32, ZYDIS_STACK_WIDTH_32);
        break;
    case ProcessorArchitecture::x64:
        ZydisDecoderInit(&m_zydisDecoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
        break;
    }

    ZydisFormatterInit(&m_zydisFormatter, ZYDIS_FORMATTER_STYLE_INTEL);
}

// Decode the instruction bytes into a human-readable string. If normalize is true,
// apply normalization to the instruction operands (i.e. map all registers to same register,
// immediate values to zero, etc.)
std::string InstructionDecoder::Decode(InstructionBytes const& instruction, bool normalize)
{
    switch (m_guestArchitecture)
    {
    case ProcessorArchitecture::x86:
    case ProcessorArchitecture::x64:
        return DecodeX86(instruction, normalize);
    case ProcessorArchitecture::Arm64:
        return DecodeArm64(instruction, normalize);
    default:
        return "<unknown architecture>";
    }
}

// Decode x86/x64 instruction bytes into a human-readable string.
std::string InstructionDecoder::DecodeX86(InstructionBytes const& instruction, bool normalize)
{
    ZydisDecodedInstruction decodedInstruction;
    std::array<ZydisDecodedOperand, ZYDIS_MAX_OPERAND_COUNT> operands;

    if (ZYAN_FAILED(ZydisDecoderDecodeFull(&m_zydisDecoder, instruction.bytes, instruction.size, &decodedInstruction, operands.data())))
    {
        return "<unknown>";
    }

    if (normalize)
    {
        Normalize(operands);
    }

    char buffer[96];
    if (ZYAN_FAILED(ZydisFormatterFormatInstruction(&m_zydisFormatter, &decodedInstruction, operands.data(),
        decodedInstruction.operand_count_visible, buffer, sizeof(buffer), 0, ZYAN_NULL)))
    {
        return "<unknown>";
    }

    return buffer;
}

// Decode ARM64 instruction bytes into a human-readable string.
std::string InstructionDecoder::DecodeArm64(InstructionBytes const& /* instruction */, bool /* normalize */)
{
    return "<decoding not implemented>";
};

// Normalize x86/x64 registers to a canonical representative.
// For example, all general purpose 32-bit registers are mapped to EAX.
// This helps to reduce the number of unique instructions when normalizing,
// while still preserving the instruction structure.
static ZydisRegister NormalizeX86Register(ZydisRegister reg)
{
    // General purpose registers  8-bit
    if (reg >= ZYDIS_REGISTER_AL && reg <= ZYDIS_REGISTER_R15B)
    {
        return ZYDIS_REGISTER_AL;
    }

    // General purpose registers 16-bit
    if (reg >= ZYDIS_REGISTER_AX && reg <= ZYDIS_REGISTER_R15W)
    {
        return ZYDIS_REGISTER_AX;
    }

    // General purpose registers 32-bit
    if (reg >= ZYDIS_REGISTER_EAX && reg <= ZYDIS_REGISTER_R15D)
    {
        return ZYDIS_REGISTER_EAX;
    }

    // General purpose registers 64-bit
    if (reg >= ZYDIS_REGISTER_RAX && reg <= ZYDIS_REGISTER_R15)
    {
        return ZYDIS_REGISTER_RAX;
    }

    // Floating point legacy registers
    if (reg >= ZYDIS_REGISTER_ST0 && reg <= ZYDIS_REGISTER_ST7)
    {
        return ZYDIS_REGISTER_ST0;
    }

    // Floating point multimedia registers
    if (reg >= ZYDIS_REGISTER_MM0 && reg <= ZYDIS_REGISTER_MM7)
    {
        return ZYDIS_REGISTER_MM0;
    }

    // Floating point vector registers 128-bit
    if (reg >= ZYDIS_REGISTER_XMM0 && reg <= ZYDIS_REGISTER_XMM31)
    {
        return ZYDIS_REGISTER_XMM0;
    }

    // Floating point vector registers 256-bit
    if (reg >= ZYDIS_REGISTER_YMM0 && reg <= ZYDIS_REGISTER_YMM31)
    {
        return ZYDIS_REGISTER_YMM0;
    }

    // Floating point vector registers 512-bit
    if (reg >= ZYDIS_REGISTER_ZMM0 && reg <= ZYDIS_REGISTER_ZMM31)
    {
        return ZYDIS_REGISTER_ZMM0;
    }

    // Matrix registers
    if (reg >= ZYDIS_REGISTER_TMM0 && reg <= ZYDIS_REGISTER_TMM7)
    {
        return ZYDIS_REGISTER_TMM0;
    }

    // Test registers
    if (reg >= ZYDIS_REGISTER_TR0 && reg <= ZYDIS_REGISTER_TR7)
    {
        return ZYDIS_REGISTER_TR0;
    }

    // Control registers
    if (reg >= ZYDIS_REGISTER_CR0 && reg <= ZYDIS_REGISTER_CR15)
    {
        return ZYDIS_REGISTER_CR0;
    }

    // Debug registers
    if (reg >= ZYDIS_REGISTER_DR0 && reg <= ZYDIS_REGISTER_DR15)
    {
        return ZYDIS_REGISTER_DR0;
    }

    // Mask registers
    if (reg >= ZYDIS_REGISTER_K0 && reg <= ZYDIS_REGISTER_K7)
    {
        return ZYDIS_REGISTER_K0;
    }

    // Bound registers
    if (reg >= ZYDIS_REGISTER_BND0 && reg <= ZYDIS_REGISTER_BND3)
    {
        return ZYDIS_REGISTER_BND0;
    }

    return reg;
}

// Normalize instruction operands for x86/x64 instructions:
// - Immediate values are set to zero.
// - Memory operands are normalized to use the first register(s) and zero offset.
// - Registers are normalized to canonical representatives.
void InstructionDecoder::Normalize(std::span<ZydisDecodedOperand> operands)
{
    for (auto& operand : operands)
    {
        if (operand.type == ZydisOperandType::ZYDIS_OPERAND_TYPE_IMMEDIATE)
        {
            // Normalize immediate values to 0
            if (operand.imm.is_signed)
            {
                operand.imm.value.s = 0;
            }
            else
            {
                operand.imm.value.u = 0;
            }
        }
        else if (operand.type == ZydisOperandType::ZYDIS_OPERAND_TYPE_MEMORY)
        {
            ZydisRegister const newRegister = m_guestArchitecture == ProcessorArchitecture::x64 ? ZYDIS_REGISTER_RAX : ZYDIS_REGISTER_EAX;

            // Normalize memory addressing to first register(s) and 0 offset
            if (operand.mem.disp.has_displacement)
            {
                operand.mem.disp.value = 0;
            }
            if (operand.mem.base != ZYDIS_REGISTER_NONE)
            {
                operand.mem.base = newRegister;
            }
            if (operand.mem.index != ZYDIS_REGISTER_NONE)
            {
                operand.mem.index = newRegister;
            }
        }
        else if (operand.type == ZydisOperandType::ZYDIS_OPERAND_TYPE_REGISTER)
        {
            operand.reg.value = NormalizeX86Register(operand.reg.value);
        }
    }
}
