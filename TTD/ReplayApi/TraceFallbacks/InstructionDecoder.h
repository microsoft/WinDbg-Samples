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
#include <span>
#include <stdint.h>
#include <string>
#include <string_view>

constexpr size_t MAX_INSTRUCTION_SIZE = 15;

struct InstructionBytes
{
    uint8_t size;
    uint8_t bytes[15];

    // For hashing.
    operator std::string_view() const
    {
        auto const p = reinterpret_cast<char const*>(this);
        return std::string_view(p, sizeof(*this));
    }
};

inline bool operator<(InstructionBytes const& a, InstructionBytes const& b) noexcept
{
    if (a.size != b.size)
    {
        return a.size < b.size;
    }
    return std::memcmp(a.bytes, b.bytes, a.size) < 0;
}

struct InstructionBytesHash
{
    size_t operator()(InstructionBytes const& a) const
    {
        std::hash<std::string_view> hasher;
        return hasher.operator()(a);
    }
};

inline bool operator==(InstructionBytes const& a, InstructionBytes const& b)
{
    using Raw = std::array<uint8_t, sizeof(InstructionBytes)>;
    return *reinterpret_cast<Raw const*>(&a) == *reinterpret_cast<Raw const*>(&b);
}

inline bool ParseHexBytes(std::string const& hexStr, InstructionBytes& instruction)
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

        char const* start = hexStr.c_str() + pos;
        char* end = nullptr;
        unsigned long byte = std::strtoul(start, &end, 16);

        if (end != start + 2 || byte > 0xFF)
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
