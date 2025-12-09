// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License. See LICENSE in the project root for license information.

// Fallbacks.h - A header defining structures and functions for analyzing TTD fallback instructions.

#pragma once

// The TTD Replay headers include a replaceable assert macro that can be used to check conditions during replay. If
// one is not defined, it defaults to no assertion checks. This sample will use the standard assert from the C runtime.
#include <assert.h>
#define DBG_ASSERT(cond) assert(cond)

#include "InstructionDecoder.h"

#include <TTD/IReplayEngine.h>

#include <filesystem>
#include <span>
#include <stdint.h>
#include <string>
#include <unordered_map>
#include <vector>

// TTD fallback types
enum class FallbackType : uint8_t
{
    FullFallback = 0,
    SyntheticInstruction = 1,
};

// Raw fallback statistics structure, used during gathering phase
struct RawFallbackInfo
{
    TTD::Replay::Position   Position{ TTD::Replay::Position::Invalid };
    uint64_t                Count{ 0 };
    FallbackType            Type{ FallbackType::FullFallback };
};

// Map of instruction bytes to raw fallback information
using FallbackStatsMap = std::unordered_map<InstructionBytes, RawFallbackInfo, InstructionBytesHash>;

// Processed fallback information structure, used for reporting
struct FallbackInfo
{
    TTD::Replay::Position   Position{ TTD::Replay::Position::Invalid };
    uint64_t                Count{ 0 };
    std::string             DecodedInstruction;
    std::string             NormalizedInstruction;
    InstructionBytes        Instruction{};
    FallbackType            Type{ FallbackType::FullFallback };
};

// Gather raw fallback statistics from the replay engine
FallbackStatsMap GatherRawFallbackInfo(TTD::Replay::IReplayEngineView& replayEngineView);

// Project rawStats into more report-friendly structure
std::vector<FallbackInfo> ProcessFallbackStats(TTD::Replay::IReplayEngineView& replayEngineView, FallbackStatsMap const& rawStats);

// Apply normalization to instruction strings
std::vector<FallbackInfo> NormalizeFallbackStats(std::span<FallbackInfo> fallbackInfo);
