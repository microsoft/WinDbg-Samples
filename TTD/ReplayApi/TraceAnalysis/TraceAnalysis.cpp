// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License. See LICENSE in the project root for license information.

// TraceAnalysis - A demonstration of data mining to extract code coverage from a trace file.
//
// Demonstrates using the TTD replay engine API to run an analysis pass over an entire
// Time Travel Debugging (TTD) trace in order to extract information (here: code coverage).
// Readers are encouraged to adapt the gathering / merging patterns shown here to compute
// other metrics (instruction mix, memory hotspots, API usage, etc.). The focus is on:
//  * Cheap, per‑event collection in a thread‑local buffer (high frequency path).
//  * Low frequency consolidation at segment boundaries.
//  * Periodic merging of completed segment results into a global aggregate.

// The TTD Replay headers include a replaceable assert macro that can be used to check conditions during replay. If
// one is not defined, it defaults to no assertion checks. This sample will use the standard assert from the C runtime.
#include <assert.h>
#define DBG_ASSERT(cond) assert(cond)

// Include the necessary headers for the TTD Replay Engine. The Formatters.h header is not required for the replay engine itself,
// but it provides custom formatters for the TTD types to work with std::format, which is used in this sample.
#include <Formatters.h>
#include <ReplayHelpers.h>
#include <TTD/IReplayEngineStl.h>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <format>
#include <future>
#include <iostream>
#include <mutex>
#include <ranges>
#include <stdexcept>
#include <vector>

// Make the sample code easier to read by bringing the TTD namespaces into scope.
using namespace TTD;
using namespace TTD::Replay;

// Number of address ranges to pre-reserve to avoid frequent reallocations in the hot path.
constexpr size_t ReservedRangesPerSegment = 0x1'0000;

// Each replay segment will invoke registered callbacks, which need to record and accumulate this data.
struct SegmentGatheredData
{
    Position                       SegmentEndPosition = Position::Invalid;
    UniqueThreadId                 Thread = UniqueThreadId::Invalid;
    std::vector<GuestAddressRange> AddressRanges;
};

// Merges adjacent and overlapping ranges in `segmentGatheredData`.
void MergeRanges(SegmentGatheredData& segmentGatheredData)
{
    
    // Sort and de-duplicate the list.
    // Duplicates are actually commonplace wherever the same code gets to run multiple times.
    // Think loops, or functions that are called multiple times.
    std::ranges::sort(segmentGatheredData.AddressRanges, [](auto const& a, auto const& b) { return a.Min < b.Min; });
    {
        auto const [begin, end] = std::ranges::unique(segmentGatheredData.AddressRanges);
        segmentGatheredData.AddressRanges.erase(begin, end);
    }

    // The merge algorithm below requires a non-empty list, so exit early if that's not the case.
    auto it = segmentGatheredData.AddressRanges.begin();
    if (it == segmentGatheredData.AddressRanges.end())
    {
        return;
    }

    // Now we can merge adjacent and overlapping ranges.
    for (auto next = std::next(it); next != segmentGatheredData.AddressRanges.end(); ++next)
    {
        if (it->Max >= next->Min)
        {
            // The `*it` and `*next` ranges overlap, so we just merge them.
            // The result remains in `*it` and `*next` is discarded.
            it->Max = next->Max;
        }
        else
        {
            // The `*it` and `*next` ranges don't overlap.
            // Move to the next `it` range and ensure it contains the `*next` range.
            ++it;
            // This is safe because at this point `it <= next`.
            // If `it < next`, then `*it` has already been merged and can be overwritten.
            *it = *next;
        }
    }
    segmentGatheredData.AddressRanges.erase(std::next(it), segmentGatheredData.AddressRanges.end());
}

// We store the accumulated data from a single segment in thread-local storage (TLS).
// This avoids the need to do any sort of synchronization in the hot path.
// Considering the potentially very high frequency of callback invocation,
// even the tiniest bit of synchronization overhead can become very significant.
static thread_local SegmentGatheredData s_segmentGatheredData{};

// This callback is invoked whenever a memory watchpoint is hit.
// For this code coverage sample, it's invoked for every single instruction executed,
// so it is very high frequency (millions to billions of calls, depending on the trace).
// In general TTD analysis and data mining algorithms will use one or more
// high-frequency callbacks just like this one.
bool __fastcall MemoryWatchpointCallback(
    uintptr_t                                  /*context*/,
    ICursorView::MemoryWatchpointResult const& watchpointResult,
    IThreadView                         const* pThreadView
) noexcept
{
    // It's good practice to find the TLS data once.
    // The compiler might not be able to optimize multiple accesses to the same TLS structure.
    auto& segmentGatheredData = s_segmentGatheredData;

    auto const position = pThreadView->GetPosition();

    if (segmentGatheredData.SegmentEndPosition == Position::Invalid)
    {
        // This is the first event in the segment, so initialize the TLS data.
        segmentGatheredData.Thread = pThreadView->GetThreadInfo().UniqueId;
        assert(segmentGatheredData.AddressRanges.empty());
        if (segmentGatheredData.AddressRanges.capacity() < ReservedRangesPerSegment)
        {
            segmentGatheredData.AddressRanges.reserve(ReservedRangesPerSegment);
        }
    }

    // We'll need the last position in the segment.
    segmentGatheredData.SegmentEndPosition = position;

    // Merge and compress the segment ranges rather than reallocating.
    // Heap operations on high-frequency code like this can hurt concurrency greatly,
    // even the constant-amortized-time reallocations of std::vector.
    // For this particular sample, this simple action has been observed to cut runtime by half.
    if (segmentGatheredData.AddressRanges.size() == segmentGatheredData.AddressRanges.capacity())
    {
        MergeRanges(segmentGatheredData);
    }

    // This is a high frequency callback, so we need to do as little work here as possible.
    // Just append the data and let the lower frequency callbacks do the expensive bits.
    segmentGatheredData.AddressRanges.emplace_back(watchpointResult.Address, watchpointResult.Address + watchpointResult.Size);

    return false; // Don't stop the replay.
}

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

	// Merge adjacent and overlapping segment ranges one last time,
    // to reduce memory overhead in the queue.
    MergeRanges(segmentGatheredData);

    // Shrink the vector's allocation as needed to reduce memory overhead.
    segmentGatheredData.AddressRanges.shrink_to_fit();

    // And enqueue the resulting range list.
    {
        std::lock_guard lock{ s_completedSegmentListMutex }; // Serialize access to global completed list.
        s_completedSegmentList.push_back(std::move(segmentGatheredData));
    }
}

// This is where we keep segment data extracted from `s_completedSegmentList` until it is ready to process.
static std::vector<SegmentGatheredData> s_gatheredSegmentList;

// The final result of the analysis gathering phase:
// the list of all address ranges that form the code coverage of the recorded process.
static std::vector<GuestAddressRange> s_gatheredAddressRanges;

// The progress callback is invoked on the same thread that invoked the replay,
// whenever the replay scheduler determines that there's a new position such that
// all the segments that finish before that position have completed their replay.
// It's meant to be used for three different purposes:
// 1. To allow for the processing of completed segments.
// 2. To provide a convenient bottleneck point to throttle the replay.
//    Without a bottleneck like this, memory use can increase unbounded.
// 3. To report progress to the user.
void __fastcall ProgressCallback(uintptr_t context, Position const& position) noexcept
{
    PositionRange const& positionRange = *reinterpret_cast<PositionRange const*>(context);
    std::cout << std::format("Progress at {:>6.02f}% position: {}\n", TTD::GetProgressPercent(position, positionRange), position);

    // When we get here, `s_completedSegmentList` is guaranteed to contain
    // the data from all the segments that ended before `position`.
    // We just need to gather them and process them.
    // Note: We will gather all the segments from the list, even those which come after `position`.
    // We will just hold on to those until a future call to this progress function.

    // We wish to preallocate enough space before entering the lock to remove the segment data.
    // Allocating memory with the lock taken can hurt concurrency significantly.
    // We do this with a quick call under the mutex.
    size_t const completedSegmentCount = []
        {
            std::lock_guard lock{ s_completedSegmentListMutex };
            return s_completedSegmentList.size();
        }();
    size_t const newGatheredSegmentListCount = s_gatheredSegmentList.size() + completedSegmentCount;

    // Reallocate now if needed.
    if (s_gatheredSegmentList.capacity() < newGatheredSegmentListCount)
    {
        s_gatheredSegmentList.reserve(newGatheredSegmentListCount);
    }

    // And extract the completed segment data.
    {
        std::lock_guard lock{ s_completedSegmentListMutex };

        // Note that we only extract the `completedSegmentCount` segments we counted earlier.
        // If we extract more, we risk having to reallocate the `s_gatheredSegmentList` vector.
        // And any new segments added between then and now would come after `position`,
        // so we can leave them for later.
        auto const begin = s_completedSegmentList.begin();
        auto const end = begin + completedSegmentCount;
        std::move(begin, end, std::back_inserter(s_gatheredSegmentList));
        s_completedSegmentList.erase(begin, end);
    }

    // Find segments that completed (SegmentEndPosition <= position).
    // Note: `std::ranges::partition` generates two subranges. The predicate selects the elements
    // that go in the first subrange but partition returns a view over the second subrange.
    // We wish to put the segments we wish to to process in the second (returned) subrange
    // for efficient erasure, so the predicate must be reversed.
    // Note: This partitioning is not really necessary if the merge is not order-sensitive,
    // but it's included here for show.
    auto const segments = std::ranges::partition(s_gatheredSegmentList, [&](auto const& data) { return data.SegmentEndPosition > position; });

    // Sort the selected elements by position.
    // Note: This is not really necessary if the merge is not order-sensitive,
    // but it's included here for show.
    std::ranges::sort(segments, [](auto const& a, auto const& b) { return a.SegmentEndPosition < b.SegmentEndPosition; });

    for (auto& segment : segments)
    {
        // Merge each segment's (already merged & de-duplicated) ranges into global coverage.
        // All ranges are sorted by "Min" address, so we can merge them efficiently in a single forward pass.
        auto it = s_gatheredAddressRanges.begin();
        for (auto& range : segment.AddressRanges)
        {
            // Skip any global coverage ranges that are strictly before this segment range.
            while (it != s_gatheredAddressRanges.end() && it->Max < range.Min)
            {
                ++it;
            }

            // There are three possibilities here...
            if (it == s_gatheredAddressRanges.end())
            {
                // 1. All remaining segment ranges are strictly after the last global range.
                // We add the current segment range to the global coverage.
                // The rest will all go through here too.
                it = s_gatheredAddressRanges.insert(it, range);
            }
            else if (range.Max < it->Min)
            {
                // 2. This segment range is strictly before the current global range.
                // We just need to insert it.
                it = s_gatheredAddressRanges.insert(it, range);
            }
            else
            {
                // We already skipped "any global coverage ranges that are strictly before this segment range",
                // so this segment range cannot be strictly after the current global range.
                // Therefore, the two ranges must overlap or at least be contiguous.
                // We must merge the segment range into the current global range.
                if (it->Min > range.Min)
                {
                    it->Min = range.Min;
                }
                if (it->Max < range.Max)
                {
                    it->Max = range.Max;
                }
                // After merging, it's possible that one or more global ranges now overlap or are contiguous,
                // so we need to find them and merge them too.
                auto next = std::next(it);
                while (next != s_gatheredAddressRanges.end() && next->Min <= it->Max)
                {
                    // For simplicity, merge forward and erase current.
                    next->Min = it->Min;
                    if (next->Max < it->Max)
                    {
                        next->Max = it->Max;
                    }
                    it = s_gatheredAddressRanges.erase(it);
                    next = std::next(it);
                }
            }
        }
    }

    // Finally, erase the segments that we just merged into global coverage.
    s_gatheredSegmentList.erase(segments.begin(), segments.end());
}

// When the progress callback is called, it's always called from the same thread
// that called the cursor's `ReplayForward` (or `ReplayBackward`) method.
// It's called by TTD's replay scheduler which launches and monitors the replay of segments,
// so the scheduler is paused until the callback returns.
// This is useful, to ensure that the scheduler doesn't get too far ahead, which would
// otherwise allow the amount of segment data pending to be merged to grow uncontrollably.
// But, if the processing done in this callback is significant, the replay scheduler
// might starve, reducing the overall throughput of the replay.
// This can be mitigated by ensuring that the callback returns quickly,
// or by offloading work to a background thread or threadpool.
// Here, we'll use std::async to demonstrate this.
static std::future<void> s_previousProgress;

// Simple, quick progress callback that schedules the merging work to be done asynchronously.
void __stdcall AsyncProgressCallback(uintptr_t context, Position const& position) noexcept
{
    // We wait for the previous async merge (if any) to complete before spawning a new one.
    if (s_previousProgress.valid())
    {
        s_previousProgress.get();
    }
    s_previousProgress = std::async(std::launch::async,
        // For any asynchronous task, always capture by value.
        [=]()
        {
            ProgressCallback(context, position);
        }
   );
}

void AnalyzeMemoryUsage(IReplayEngineView& replayEngine)
{
    UniqueCursor pOwnedCursor{ replayEngine.NewCursor() };
    if (!pOwnedCursor)
    {
        throw std::runtime_error("Failed to create a replay engine cursor");
    }

    // In order to gather data efficiently from multithreaded replay
    // we need to route it through these two callbacks.

    // The thread continuity callback gathers data from TLS,
    // accumulated from the replay of a single segment.
    pOwnedCursor->SetThreadContinuityBreakCallback(ThreadContinuityCallback, 0);

    // Get the position lifetime of the entire trace.
    auto const& positionRange = replayEngine.GetLifetime();

    // The progress callback gathers this data from completed segments
    // and merges it to construct the result of the analysis.
    pOwnedCursor->SetReplayProgressCallback(AsyncProgressCallback, reinterpret_cast<uintptr_t>(&positionRange)); // Use async to keep replay thread(s) moving.
    // Alternative (synchronous) option if simplicity > throughput:
    // pOwnedCursor->SetReplayProgressCallback(ProgressCallback, reinterpret_cast<uintptr_t>(&positionRange));

    // Note: The watchpoint covers all valid addresses, so this is strictly not needed.
    // No segments would get filtered out.
    // But it's still a good idea to set the flag explicitly for clarity.
    pOwnedCursor->SetReplayFlags(ReplayFlags::ReplayAllSegmentsWithoutFiltering);

    // Set code execution watchpoint across entire address space
    pOwnedCursor->AddMemoryWatchpoint(
        MemoryWatchpointData
        {
            .Address    = GuestAddress{ 0 },
            .Size       = UINT64_MAX,
            .AccessMask = DataAccessMask::Execute,
        }
    );
    pOwnedCursor->SetMemoryWatchpointCallback(MemoryWatchpointCallback, 0);
    pOwnedCursor->SetEventMask(EventMask::MemoryWatchpoint);

    // And replay the entire trace.
    // Note: ReplayResult is a member of ICursorView so you have to refer to it in that way,
    //       or use auto.
    pOwnedCursor->SetPosition(Position::Min);
    ICursorView::ReplayResult const result = pOwnedCursor->ReplayForward();
    if (result.StopReason == EventType::Error)
    {
        std::cout << "Replay Error!\n";
    }

    // Wait for the last async merge (if any) to complete before spawning a new one.
    if (s_previousProgress.valid())
    {
        s_previousProgress.get();
    }

    // Force final merge of any remaining gathered segments.
    ProgressCallback(reinterpret_cast<uintptr_t>(&positionRange), replayEngine.GetLastPosition());

    // And report the raw coverage data we gathered.

    // When listing ranges, we'll list just this many as a sample, to keep the output short.
    static constexpr size_t ReportLimit = 20;

    std::cout << std::format("Found {} distinct coverage memory ranges\n", s_gatheredAddressRanges.size());
    for (auto& range : std::views::take(s_gatheredAddressRanges, ReportLimit))
    {
        std::cout << std::format("    0x{:X} - 0x{:X}\n", range.Min, range.Max);
    }
    if (s_gatheredAddressRanges.size() > ReportLimit)
    {
        std::cout << "    ...\n";
    }

    if (!s_gatheredAddressRanges.empty())
    {
        // Report the same ranges, but grouped by proximity and with gaps summarized.
        // Note: There's nothing TTD-specific here, but when reporting results
        // it's generally a good idea to summarize it in ways that make sense for the data.
        // This is just an example of doing that.
        struct RangeWithGaps
        {
            GuestAddressRange Range;
            uint64_t          GapCount = 0;
            uint64_t          GapBytes = 0;
        };
        // We start with the first range with no gaps.
        std::vector<RangeWithGaps> rangesWithGaps{ RangeWithGaps{ .Range = s_gatheredAddressRanges.front() } };
        // And iterate over the rest.
        for (auto& range : std::views::drop(1)(s_gatheredAddressRanges))
        {
            RangeWithGaps& current = rangesWithGaps.back();

            // This is the heuristic to group nearby ranges with small gaps.
            // We'll be unconditionally merging all ranges that are less than 64 bytes from the previous range.
            // And also all the ranges that are less than a page from the previous and keep
            // the total bytes of gaps significantly less than the total size of the range.
            uint64_t const newTotalBytes = static_cast<uint64_t>(range.Max - current.Range.Min);
            uint64_t const addedGapBytes = static_cast<uint64_t>(range.Min - current.Range.Max);
            uint64_t const newGapBytes = current.GapBytes + addedGapBytes;
            if (addedGapBytes < 64 || (addedGapBytes < 4096 && newGapBytes <= newTotalBytes / 4))
            {
                // Merge the ranges.
                if (addedGapBytes > 0)
                {
                    ++current.GapCount;
                }
                current.GapBytes = newGapBytes;
                current.Range.Max = range.Max;
            }
            else
            {
                // New range.
                rangesWithGaps.push_back(RangeWithGaps{ .Range = range });
            }
        }

        // And report the grouped results.
        std::cout << std::format("{} coverage memory ranges with some gaps\n", rangesWithGaps.size());
        for (RangeWithGaps& rangeWithGaps : std::views::take(rangesWithGaps, ReportLimit))
        {
            std::cout << std::format("    0x{:X} - 0x{:X} {} bytes with {} gap bytes in {} gaps\n",
                rangeWithGaps.Range.Min,
                rangeWithGaps.Range.Max,
                rangeWithGaps.Range.Max - rangeWithGaps.Range.Min,
                rangeWithGaps.GapBytes,
                rangeWithGaps.GapCount
            );
        }
        if (rangesWithGaps.size() > ReportLimit)
        {
            std::cout << "    ...\n";
        }
    }
}

int main(int argc, char const* const argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: TraceAnalysis <trace file>\n";
        return 1;
    }

    std::filesystem::path traceFile = argv[1];

    auto [pOwnedReplayEngine, createResult] = MakeReplayEngine();
    if (createResult != 0 || pOwnedReplayEngine == nullptr)
    {
        std::cout << std::format("There was an issue creating a replay engine ({})\n", createResult);
        return -1;
    }

    if (!pOwnedReplayEngine->Initialize(traceFile.c_str()))
    {
        std::cout << std::format("Failed to initialize the engine\n");
        return -1;
    }

    try
    {
        AnalyzeMemoryUsage(*pOwnedReplayEngine);
    }
    catch (std::exception const& e)
    {
        std::cout << std::format("Error: {}\n", e.what());
        return -1;
    }

    return 0;
}
