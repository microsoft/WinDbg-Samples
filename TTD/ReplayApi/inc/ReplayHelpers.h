// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License. See LICENSE in the project root for license information.

// ReplayHelpers.h - A collection of utility functions and types that are
// useful when working with the TTD Replay Engine.

#pragma once

#include <TTD/IdnaBasicTypes.h>
#include <TTD/IReplayEngine.h>

#define NOMINMAX
#include <Windows.h>

#include <type_traits>
#include <utility>
#include <wchar.h>

namespace TTD
{

// Get the architecture of the trace being replayed.
inline ProcessorArchitecture GetGuestArchitecture(Replay::IReplayEngineView& replayEngine)
{
    SystemInfo const& systemInfo = replayEngine.GetSystemInfo();
    switch (systemInfo.System.ProcessorArchitecture)
    {
    case PROCESSOR_ARCHITECTURE_INTEL: return ProcessorArchitecture::x86;
    case PROCESSOR_ARCHITECTURE_AMD64: return ProcessorArchitecture::x64;
    case PROCESSOR_ARCHITECTURE_ARM64: return ProcessorArchitecture::Arm64;
    default: return ProcessorArchitecture::Invalid;
    }
}

// Get the architecture of the trace being replayed.
inline ProcessorArchitecture GetGuestArchitecture(Replay::ICursorView& cursor)
{
    return GetGuestArchitecture(*cursor.GetReplayEngine());
}

// Compute how far into the replay the given position is within the range.
inline double GetProgressPercent(Replay::Position const& position, Replay::PositionRange range)
{
    if (range.Max < range.Min)
    {
        std::swap(range.Max, range.Min);
    }

    if (position <= range.Min)
    {
        return 0.0;
    }

    if (position >= range.Max)
    {
        return 100.0;
    }

    auto const sequenceDelta = range.Max.Sequence - range.Min.Sequence;
    if (sequenceDelta == 0)
    {
        return 0.0;
    }
    auto const progressDelta = position.Sequence - range.Min.Sequence;
    auto const progressPercent = progressDelta * 100.0 / sequenceDelta;
    return progressPercent;
}

enum class ReplayDirection
{
    Forward,
    Backward,
};

constexpr char const* GetReplayDirectionName(ReplayDirection direction)
{
    switch (direction)
    {
    case ReplayDirection::Forward:
        return "Forward";
    case ReplayDirection::Backward:
        return "Backward";
    default:
        return "Unknown";
    }
}

struct WatchpointQueryResult
{
    Replay::Position    Position;
    Replay::Position    PreviousPosition;
};

// Replay the trace in the given range and direction. The callback can be a lambda for
// the following events:
// -   Memory watchpoint: [](TTD::Replay::ICursorView::MemoryWatchpointResult const& watchpoint, TTD::Replay::IThreadView const* pThreadView) -> bool {}
// - Position watchpoint: [](TTD::Replay::Position position, TTD::Replay::IThreadView const* pThreadView) -> bool {}
// It can also be a callable object providing the following functions (all optional):
// -   Memory watchpoint: bool operator()(TTD::Replay::ICursorView::MemoryWatchpointResult const& watchpoint, TTD::Replay::IThreadView const* pThreadView);
// - Position watchpoint: bool operator()(TTD::Replay::Position position, TTD::Replay::IThreadView const* pThreadView);
// -            Progress: bool Progress(TTD::Replay::Position position, double positionPercent);
//                             return false to interrupt the replay.
// -   Thread continuity: void ThreadContinuity();
// It is assumed the caller has already added the necessary watchpoints before calling FilteredWatchpointQuery().
// NOTE: The cursor is mutated during the call to FilteredWatchpointQuery() but will be restored to extent possible.
template <typename Callback>
WatchpointQueryResult FilteredWatchpointQuery(
    Replay::ICursorView& cursor,
    Replay::PositionRange               replayRange,
    ReplayDirection                     direction,
    Callback& callback
)
{
    struct PreserveCursor
    {
        PreserveCursor(Replay::ICursorView& cursor) :
            Cursor(cursor)
        {
            Position = Cursor.GetPosition();
            ReplayFlags = Cursor.GetReplayFlags();
            EventMask = Cursor.GetEventMask();
        }

        ~PreserveCursor()
        {
            // Restore values that were changed by FilteredWatchpointQuery
            Cursor.SetReplayFlags(ReplayFlags);
            auto const eventMask = Cursor.GetEventMask();
            Cursor.SetEventMask(EventMask);

            // Remove the memory watchpoint callback if it was set
            if ((eventMask & Replay::EventMask::MemoryWatchpoint) == Replay::EventMask::MemoryWatchpoint)
            {
                Cursor.SetMemoryWatchpointCallback(nullptr, 0);
            }

            // Remove the position watchpoint callback if it was set
            if ((eventMask & Replay::EventMask::PositionWatchpoint) == Replay::EventMask::PositionWatchpoint)
            {
                Cursor.SetPositionWatchpointCallback(nullptr, 0);
            }

            // Remove the thread continuity callback if it was set
            if constexpr (requires { Cursor.SetThreadContinuityBreakCallback(nullptr, 0); })
            {
                Cursor.SetThreadContinuityBreakCallback(nullptr, 0);
            }

            Cursor.SetReplayProgressCallback(nullptr, 0);

            // Do this after restoring the Cursor callbacks
            Cursor.SetPosition(Position);
        }

        Replay::ICursorView& Cursor;
        Replay::Position        Position;
        Replay::ReplayFlags     ReplayFlags;
        Replay::EventMask       EventMask;
    } preserveCursor(cursor);

    if (direction == ReplayDirection::Backward)
    {
        std::swap(replayRange.Min, replayRange.Max);
    }

    cursor.SetPosition(replayRange.Min);

    auto eventMask = Replay::EventMask::None;
    // Set a memory watchpoint callback if the filter function accepts it.
    if constexpr (requires(Replay::ICursorView::MemoryWatchpointResult watchpoint, Replay::IThreadView const* pThreadView) { callback(watchpoint, pThreadView); })
    {
        cursor.SetMemoryWatchpointCallback(
            []([[maybe_unused]] uintptr_t context, auto watchpoint, [[maybe_unused]] auto pThreadView)
            {
                auto& callback = *reinterpret_cast<Callback*>(context);
                return callback(watchpoint, pThreadView);
            },
            reinterpret_cast<uintptr_t>(&callback)
        );
        eventMask |= Replay::EventMask::MemoryWatchpoint;
    }

    // Set a position watchpoint callback if the filter function accepts it.
    if constexpr (requires(Replay::Position position, Replay::IThreadView const* pThreadView) { callback(position, pThreadView); })
    {
        cursor.SetPositionWatchpointCallback(
            []([[maybe_unused]] uintptr_t context, auto watchpoint, [[maybe_unused]] auto pThreadView)
            {
                auto& callback = *reinterpret_cast<Callback*>(context);
                return callback(watchpoint, pThreadView);
            },
            reinterpret_cast<uintptr_t>(&callback)
        );
        eventMask |= Replay::EventMask::PositionWatchpoint;
    }

    // Set a thread continuity callback if the filter function accepts it.
    if constexpr (requires { callback.ThreadContinuity(); })
    {
        cursor.SetThreadContinuityBreakCallback(
            []([[maybe_unused]] uintptr_t context)
            {
                auto& callback = *reinterpret_cast<Callback*>(context);
                callback.ThreadContinuity();
            },
            reinterpret_cast<uintptr_t>(&callback)
        );
    }

    cursor.SetEventMask(eventMask);

    auto const replayProgress = [&]([[maybe_unused]] Replay::Position const& position)
        {
            if constexpr (requires { { callback.Progress(Replay::Position{}, double{}) } -> std::same_as<bool>; })
            {
                auto const progressPercent = GetProgressPercent(position, replayRange);
                if (callback.Progress(position, progressPercent))
                {
                    cursor.InterruptReplay();
                }
            }
        };
    cursor.SetReplayProgressCallback(replayProgress);

    for (;;)
    {
        auto const previousPosition = cursor.GetPosition();

        auto const replayResult = [&]()
            {
                if (direction == ReplayDirection::Forward)
                {
                    return cursor.ReplayForward(replayRange.Max);
                }
                else
                {
                    DBG_ASSERT(direction == ReplayDirection::Backward);
                    return cursor.ReplayBackward(replayRange.Max);
                }
            }();

        if (replayResult.StopReason == Replay::EventType::Process
            || replayResult.StopReason == Replay::EventType::Position
            || replayResult.StopReason == Replay::EventType::MemoryWatchpoint
            || replayResult.StopReason == Replay::EventType::Interrupted)
        {
            break;
        }

        auto const newPosition = cursor.GetPosition();
        if (newPosition == previousPosition)
        {
            break;
        }
    }

    return { cursor.GetPosition(), cursor.GetPreviousPosition() };
}

// Determine the range of positions to operate on based on the current position
// and desired replay direction. When replaying forward, the range is from the
// current position + 1 to the end of the trace. When replaying backward, the
// range is from the beginning of the trace to the current position - 1.
inline Replay::PositionRange GetReplayRange(Replay::ICursorView& cursor, ReplayDirection direction)
{
    if (direction == ReplayDirection::Forward)
    {
        // Construct range from position + 1 to end of trace
        return Replay::PositionRange
        {
            cursor.GetPosition() + Replay::StepCount{1},
            cursor.GetReplayEngine()->GetLifetime().Max,
        };
    }

    // Construct range from beginning of trace to current position - 1
    auto replayRange = Replay::PositionRange
    {
        cursor.GetReplayEngine()->GetLifetime().Min,
        cursor.GetPosition(),
    };

    if (replayRange.Max.Steps != 0)
    {
        replayRange.Max.Steps -= 1;
    }
    else if (replayRange.Max.Sequence > replayRange.Min.Sequence)
    {
        replayRange.Max.Sequence -= 1;
        replayRange.Max.Steps = Replay::StepCount::Max;
    }

    return replayRange;
}

// Parses a string representation of a position and returns the corresponding position.
// If the string is not a valid position, the default value is returned.
inline TTD::Replay::Position TryParsePositionFromString(
    _In_z_ wchar_t               const* pStr,
    _In_   TTD::Replay::Position const& def)
{
    if (pStr == nullptr || *pStr == L'\0')
    {
        return def;
    }

    if (_wcsicmp(pStr, L"min") == 0)
    {
        return TTD::Replay::Position::Min;
    }

    if (_wcsicmp(pStr, L"max") == 0)
    {
        return TTD::Replay::Position::Max;
    }

    if (_wcsicmp(pStr, L"invalid") == 0)
    {
        return TTD::Replay::Position::Invalid;
    }

    // Skip leading zeros.
    while (*pStr == L'0')
    {
        ++pStr;
    }

    constexpr size_t c_uint64HexDigits = 16;
    constexpr size_t c_positionStringMaxLength = c_uint64HexDigits * 2; // Sequence + steps = 32 digits.
    wchar_t arg[c_positionStringMaxLength + 1];
    size_t len = 0;
    size_t seqEnd = 0;
    while (len < c_positionStringMaxLength)
    {
        if ((*pStr >= L'0' && *pStr <= L'9') ||
            (*pStr >= L'A' && *pStr <= L'F') ||
            (*pStr >= L'a' && *pStr <= L'f'))
        {
            arg[len] = *pStr;
            ++len;
        }
        else if (*pStr == L'`' || *pStr == L'\'')
        {
        }
        else if (len > 0 && *pStr == L':')
        {
            seqEnd = len;
        }
        else
        {
            break;
        }
        ++pStr;
    }
    if (*pStr != L'\0')
    {
        return def;
    }
    arg[len] = 0;
    TTD::Replay::Position position;
    position.Steps = static_cast<TTD::Replay::StepCount>(wcstoull(arg + seqEnd, nullptr, 16));
    arg[seqEnd] = L'\0';
    position.Sequence = static_cast<TTD::SequenceId>(wcstoull(arg, nullptr, 16));
    return position;
}

// Convert a floating point percentage to a position within the given range. Currently
// the percentage is applied to the sequence number only; the step count is always 0.
// Note that doing a round trip conversion from floating point to position and back
// may not yield the same percentage due to floating point precision issues.
inline TTD::Replay::Position TryParsePositionFromPercentage(
    TTD::Replay::PositionRange const& range,
    float                             percentage)
{
    if (percentage <= 0.0f)
    {
        return range.Min;
    }
    if (percentage >= 100.0f)
    {
        return range.Max;
    }

    return range.Min.Sequence + static_cast<uint64_t>((range.Max.Sequence - range.Min.Sequence) * percentage / 100.0f);
}

// Get the range of positions in the trace.
inline TTD::Replay::PositionRange GetTracePositionRange(TTD::Replay::IReplayEngineView const& engine)
{
    return { engine.GetFirstPosition(), engine.GetLastPosition() };
}

// Convert a block of bytes to a hex string representation, up to a maximum number of bytes.
template < size_t MaxBytes >
inline std::string GetBytesString(
    _In_                        size_t            dataSizeInBytes,
    _In_reads_(dataSizeInBytes) void const* const pData
)
{
    if (dataSizeInBytes > MaxBytes)
    {
        dataSizeInBytes = MaxBytes;
    }

    if (dataSizeInBytes == 0 || pData == nullptr)
    {
        return {};
    }

    char buffer[MaxBytes * 3 + 1];
    for (size_t i = 0; i < dataSizeInBytes; ++i)
    {
        snprintf(&buffer[i * 3], 4, "%02X ", static_cast<uint8_t const*>(pData)[i]);
    }
    return { buffer, dataSizeInBytes * 3 - 1 };
}

} // namespace TTD
