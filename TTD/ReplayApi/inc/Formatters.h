// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License. See LICENSE in the project root for license information.

// Formatters.h - Custom formatters for TTD types, for use with std::format().

#pragma once

#include <TTD/IReplayEngine.h>

#include <format>
#include <stdint.h>
#include <string>

// Custom formatter for TTD::GuestAddress to work with std::format
template < typename CharT >
struct std::formatter<TTD::GuestAddress, CharT> : std::formatter<uint64_t, CharT> {
    template <typename FormatContext>
    auto format(const TTD::GuestAddress& addr, FormatContext& ctx) const {
        return std::formatter<uint64_t, CharT>::format(static_cast<uint64_t>(addr), ctx);
    }
};

// Custom formatter for TTD::Replay::TTD::Replay::UniqueThreadId to work with std::format
template < typename CharT >
struct std::formatter<TTD::Replay::UniqueThreadId, CharT> : std::formatter<uint32_t, CharT> {
    template <typename FormatContext>
    auto format(const TTD::Replay::UniqueThreadId& tid, FormatContext& ctx) const {
        return std::formatter<uint32_t, CharT>::format(static_cast<uint32_t>(tid), ctx);
    }
};

// Custom formatter for TTD::Replay::SequenceId to work with std::format
template < typename CharT >
struct std::formatter<TTD::SequenceId, CharT> : std::formatter<uint64_t, CharT> {
    template <typename FormatContext>
    auto format(const TTD::SequenceId& seq, FormatContext& ctx) const {
        return std::formatter<uint64_t, CharT>::format(static_cast<uint64_t>(seq), ctx);
    }
};

// Custom formatter for TTD::Replay::StepCount to work with std::format
template < typename CharT >
struct std::formatter<TTD::Replay::StepCount, CharT> : std::formatter<uint64_t, CharT> {
    template <typename FormatContext>
    auto format(const TTD::Replay::StepCount& step, FormatContext& ctx) const {
        return std::formatter<uint64_t, CharT>::format(static_cast<uint64_t>(step), ctx);
    }
};

// Custom formatter for TTD::InstructionCount to work with std::format
template < typename CharT >
struct std::formatter<TTD::InstructionCount, CharT> : std::formatter<uint64_t, CharT> {
    template <typename FormatContext>
    auto format(const TTD::InstructionCount& instructionCount, FormatContext& ctx) const {
        return std::formatter<uint64_t, CharT>::format(static_cast<uint64_t>(instructionCount), ctx);
    }
};

// Custom formatter for TTD::Replay::Position to work with std::format
template < typename CharT >
struct std::formatter<TTD::Replay::Position, CharT> : std::formatter<std::basic_string<CharT>, CharT> {
    template <typename FormatContext>
    auto format(const TTD::Replay::Position& pos, FormatContext& ctx) const {
        if constexpr (std::is_same_v<CharT, wchar_t>)
        {
            return std::formatter<std::basic_string<CharT>, CharT>::format(
                std::format(L"{:X}:{:X}", pos.Sequence, pos.Steps), ctx);
        }
        else
        {
            return std::formatter<std::basic_string<CharT>, CharT>::format(
                std::format("{:X}:{:X}", pos.Sequence, pos.Steps), ctx);
        }
    }
};

// Custom formatter for TTD::Replay::PositionRange to work with std::format
template < typename CharT >
struct std::formatter<TTD::Replay::PositionRange, CharT> : std::formatter<std::basic_string<CharT>, CharT> {
    template <typename FormatContext>
    auto format(const TTD::Replay::PositionRange& range, FormatContext& ctx) const {
        if constexpr (std::is_same_v<CharT, wchar_t>)
        {
            return std::formatter<std::basic_string<CharT>, CharT>::format(
                std::format(L"{}-{}", range.Min, range.Max), ctx);
        }
        else
        {
            return std::formatter<std::basic_string<CharT>, CharT>::format(
                std::format("{}-{}", range.Min, range.Max), ctx);
        }
    }
};
