// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License. See LICENSE in the project root for license information.

// Fallbacks.cpp - A source file implementing structures and functions for analyzing TTD fallback instructions.

#include "Fallbacks.h"

// The TTD Replay headers include a replaceable assert macro that can be used to check conditions during replay. If
// one is not defined, it defaults to no assertion checks. This sample will use the standard assert from the C runtime.
#include <assert.h>
#define DBG_ASSERT(cond) assert(cond)

#include <TTD/IReplayEngineStl.h>
#include "ReplayHelpers.h"
#include "Formatters.h"

#include <exception>
#include <format>
#include <iostream>
#include <mutex>

// Make the sample code easier to read by bringing the TTD namespaces into scope.
using namespace TTD;
using namespace TTD::Replay;

// Each replay segment will invoke registered callbacks, which need to record and accumulate this data.
struct SegmentGatheredData
{
    FallbackStatsMap    Stats;
};

// We store the accumulated data from a single segment in thread-local storage (TLS).
// This avoids the need to do any sort of synchronization in the hot path.
// Considering the potentially very high frequency of callback invocation,
// even the tiniest bit of synchronization overhead can become very significant.
static thread_local SegmentGatheredData s_segmentGatheredData{};

// The data for multiple segments needs to be gathered into a single list for processing.
// Each segment is replayed in a different thread, so a mutex is needed for synchronization.
static std::mutex                       s_completedSegmentListMutex;
static std::vector<SegmentGatheredData> s_completedSegmentList;

// The thread continuity callback is invoked at the end of each segment, on the same thread that replayed the segment.
// Its purpose is to get the data extracted by the high-frequency callbacks during replay of the segment,
// compress, optimize or summarize it as appropriate, and add it to the global list.
void __fastcall ThreadContinuityCallback(uintptr_t /*context*/) noexcept
{
    // The segment ended, so destructively remove and reset the TLS data.
    // This is generally good practice because it avoids leaving potentially large
    // data structures stuck in TLS for threads that might never use it again.
    // We may wish to recycle objects or data buffers between segments,
    // but in that case we should do that in a different way rather than leaving it
    // in the TLS structure in case the thread replays a new segment.
    SegmentGatheredData segmentGatheredData = std::exchange(s_segmentGatheredData, {});

    {
        std::lock_guard lock{ s_completedSegmentListMutex }; // Serialize access to global completed list.
        s_completedSegmentList.push_back(std::move(segmentGatheredData));
    }
}

// Gather raw fallback statistics from the replay engine, using the FallbackCallback.
// For efficiency, we gather data per-segment in thread-local storage (TLS),
// and then merge the per-segment data into a single map at the end.
FallbackStatsMap GatherRawFallbackInfo(IReplayEngineView& replayEngineView)
{
    UniqueCursor pOwnedCursor{ replayEngineView.NewCursor() };
    if (!pOwnedCursor)
    {
        throw std::runtime_error("Failed to create a replay engine cursor");
    }

    auto fallbackCallback = [&](bool synthetic, GuestAddress pc, size_t size, IThreadView const* pThreadView) {
        // It's good practice to find the TLS data once.
        // The compiler might not be able to optimize multiple accesses to the same TLS structure.
        auto& segmentGatheredData = s_segmentGatheredData;

        InstructionBytes fallback{};
        DBG_ASSERT(size <= _countof(fallback.bytes));

        fallback.size = static_cast<uint8_t>(size);
        auto const queryResult = pThreadView->QueryMemoryBuffer(pc, BufferView{ fallback.bytes, size });

        FallbackType const fallbackType = synthetic ? FallbackType::SyntheticInstruction : FallbackType::FullFallback;

        Position const position = pThreadView->GetPosition();

        InstructionBytes const& key = queryResult.Memory.Size == size ? fallback : InstructionBytes{};
        
        RawFallbackInfo const value = {
            .Position = position,
            .Count = 1,
            .Type = fallbackType,
        };

        if (auto [iter, inserted] = segmentGatheredData.Stats.insert({ key, value }); !inserted)
        {
            iter->second.Count += 1;
        }
    };
    pOwnedCursor->SetFallbackCallback(fallbackCallback);

    pOwnedCursor->SetThreadContinuityBreakCallback(ThreadContinuityCallback, 0);

    PositionRange const lifetime = replayEngineView.GetLifetime();
    auto progressCallback = [&](Replay::Position const& position)
    {
        std::cout << std::format("\rProgress at {:>6.02f}% position: {}", TTD::GetProgressPercent(position, lifetime), position);
    };
    pOwnedCursor->SetReplayProgressCallback(progressCallback);

    pOwnedCursor->SetPosition(replayEngineView.GetLifetime().Min);
    pOwnedCursor->SetReplayFlags(ReplayFlags::ReplayAllSegmentsWithoutFiltering);
    pOwnedCursor->ReplayForward();

    progressCallback(replayEngineView.GetLifetime().Max);
    std::cout << "\n";

    // Merge all segment stats into a single map from s_completedSegmentList
    FallbackStatsMap mergedStats;
    {
        std::lock_guard lock{ s_completedSegmentListMutex }; // Serialize access to global completed list.
        for (auto& segmentData : s_completedSegmentList)
        {
            for (auto const& [instruction, info] : segmentData.Stats)
            {
                if (auto [iter, inserted] = mergedStats.insert({ instruction, info }); !inserted)
                {
                    iter->second.Count += info.Count;
                    iter->second.Position = std::min(iter->second.Position, info.Position);
                }
            }
        }
        s_completedSegmentList.clear();
    }

    return mergedStats;
}

// Project rawStats into more report-friendly structure
std::vector<FallbackInfo> ProcessFallbackStats(IReplayEngineView& replayEngineView, FallbackStatsMap const& rawStats)
{
    auto const guestArchitecture = TTD::GetGuestArchitecture(replayEngineView);
    InstructionDecoder decoder{ guestArchitecture };

    std::vector<FallbackInfo> processedStats;
    processedStats.reserve(rawStats.size());

    for (auto const& [instruction, info] : rawStats)
    {
        processedStats.push_back(FallbackInfo{
            .Position = info.Position,
            .Count = info.Count,
            .DecodedInstruction = decoder.Decode(instruction, false),
            .NormalizedInstruction = decoder.Decode(instruction, true),
            .Instruction = instruction,
            .Type = info.Type,
        });
    }

    return processedStats;
}

std::vector<FallbackInfo> NormalizeFallbackStats(std::span<FallbackInfo> fallbackInfo)
{
    // Insert all fallbacks into a map keyed by normalized instruction
    // to aggregate counts of fallbacks that decode to the same normalized instruction
    // then produce a new vector from the map
    std::unordered_map<std::string, FallbackInfo> normalizedMap;
    for (auto const& info : fallbackInfo)
    {
        auto [iter, inserted] = normalizedMap.try_emplace(info.NormalizedInstruction, info);
        if (!inserted)
        {
            // Entry already exists, just update the count
            iter->second.Count += info.Count;
        }
    }
    std::vector<FallbackInfo> normalizedStats;
    normalizedStats.reserve(normalizedMap.size());
    for (auto const& [_, info] : normalizedMap)
    {
        normalizedStats.push_back(info);
    }
    return normalizedStats;
}
