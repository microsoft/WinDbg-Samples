// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License. See LICENSE in the project root for license information.

// InstructionDecoder.h - A utility class for decoding CPU instructions from their byte representation.

#pragma once

// The TTD Replay headers include a replaceable assert macro that can be used to check conditions during replay. If
// one is not defined, it defaults to no assertion checks. This sample will use the standard assert from the C runtime.
#include <assert.h>
#define DBG_ASSERT(cond) assert(cond)

#include <TTD/IReplayEngine.h>

#include <zydis/Zydis.h> // for instruction decoding

#include <array>
#include <charconv>
#include <compare>
#include <cstring>
#include <functional>
#include <span>
#include <stdint.h>
#include <string>
#include <string_view>
#include <system_error>

constexpr size_t MAX_INSTRUCTION_SIZE = 15;

struct InstructionBytes
{
    uint8_t size;
    uint8_t bytes[MAX_INSTRUCTION_SIZE];

    // Custom equality and ordering that only compares the first 'size' bytes
    bool operator==(InstructionBytes const& other) const
    {
        if (size != other.size)
        {
            return false;
        }
        return std::memcmp(bytes, other.bytes, size) == 0;
    }
    
    std::strong_ordering operator<=>(InstructionBytes const& other) const
    {
        int result = std::memcmp(bytes, other.bytes, size);
        if (result < 0) return std::strong_ordering::less;
        if (result > 0) return std::strong_ordering::greater;
        return std::strong_ordering::equal;
    }
};

struct InstructionBytesHash
{
    size_t operator()(InstructionBytes const& a) const
    {
        // Use FNV-1a hash algorithm to hash the instruction bytes
        constexpr size_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
        constexpr size_t FNV_PRIME = 1099511628211ULL;
        
        size_t hash = FNV_OFFSET_BASIS;
        
        // Hash the size first
        hash ^= static_cast<size_t>(a.size);
        hash *= FNV_PRIME;
        
        // Hash each byte of the instruction
        for (uint8_t i = 0; i < a.size; ++i)
        {
            hash ^= static_cast<size_t>(a.bytes[i]);
            hash *= FNV_PRIME;
        }
        
        return hash;
    }
};

inline bool ParseHexBytes(std::string_view hexStr, InstructionBytes& instruction)
{
    instruction.size = 0;

    // Parse hex string like "48 8b 05 12 34 56 78"
    size_t pos = 0;
    while (pos < hexStr.length() && instruction.size < MAX_INSTRUCTION_SIZE)
    {
        // Skip whitespace
        while (pos < hexStr.length() && std::isspace(static_cast<unsigned char>(hexStr[pos])))
        {
            ++pos;
        }

        if (pos >= hexStr.length())
        {
            break;
        }

        // Parse two hex digits
        if (pos + 1 >= hexStr.length())
        {
            return false;
        }

        unsigned char byte;
        auto [_, ec] = std::from_chars(hexStr.data(), hexStr.data() + 2, byte, 16);
        if (ec != std::errc())
        {
            return false;
        }

        instruction.bytes[instruction.size++] = static_cast<uint8_t>(byte);
        pos += 2;
    }

    return instruction.size > 0;
}

// A utility class for decoding CPU instructions from their byte representation.
// It caches decoder and formatter state for efficiency.
class InstructionDecoder
{
public:
    InstructionDecoder(TTD::ProcessorArchitecture guestArchitecture);

    // Decode the instruction bytes into a human-readable string. If normalize is true,
    // apply normalization to the instruction operands (i.e. map all registers to same register,
    // immediate values to zero, etc.)
    std::string Decode(InstructionBytes const&, bool normalize);

private:
    std::string DecodeX86(InstructionBytes const&, bool normalize);
    std::string DecodeArm64(InstructionBytes const&, bool normalize);

    void Normalize(std::span<ZydisDecodedOperand> operands);

private:
    TTD::ProcessorArchitecture  m_guestArchitecture;

    ZydisDecoder                m_zydisDecoder;
    ZydisFormatter              m_zydisFormatter;
};
