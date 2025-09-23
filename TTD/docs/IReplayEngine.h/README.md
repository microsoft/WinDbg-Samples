# TTD/IReplayEngine.h

This header defines the main interface for TTD's Replay Engine, providing comprehensive APIs for loading and analyzing time travel debugging trace files. The replay engine enables navigation through execution timelines, memory inspection, and breakpoint management.

It resides in the [Microsoft.TimeTravelDebugging.Apis](../Microsoft.TimeTravelDebugging.Apis.md) NuGet package.

## Core Concepts

### Trace File
Contains all or portions of the execution of a single process between two points of time, including:
- CPU instructions executed
- Approximate contents of the process' memory
- Relevant events (exceptions, debugger output, DLL load/unload, thread creation/termination)

### Timeline
The complete list of CPU instructions executed between the begin and end points recorded in the trace. All other events are attached to specific instruction executions.

### Position
128-bit number identifying a single instruction executed in the trace's timeline. Positions are monotonically increasing and can be compared numerically.

## Primary Interfaces

- [`IReplayEngineView`](interface-IReplayEngineView.md) - Main replay engine view interface
- [`IReplayEngine`](interface-IReplayEngine.md) - Owning replay engine interface
- [`ICursorView`](interface-ICursorView.md) - Cursor view for timeline navigation
- [`ICursor`](interface-ICursor.md) - Owning cursor interface
- [`ITraceListView`](interface-ITraceListView.md) - Trace list view interface
- [`ITraceList`](interface-ITraceList.md) - Owning trace list interface
- [`IThreadView`](interface-IThreadView.md) - Thread-specific view interface

## Core Data Structures

### Position and Timeline
- [`Position`](struct-Position.md) - 128-bit timeline position identifier
- [`PositionRange`](struct-PositionRange.md) - Range of positions
- [`StepCount`](enum-StepCount.md) - Number of execution steps
- [`SequenceId`](enum-SequenceId.md) - Sequence identifier

### Register and Context
- [`RegisterContext`](union-RegisterContext.md) - Cross-platform register context
- [`ExtendedRegisterContext`](union-ExtendedRegisterContext.md) - Extended SIMD registers

### Memory Management
- [`MemoryRange`](struct-MemoryRange.md) - Memory region descriptor
- [`MemoryBuffer`](struct-MemoryBuffer.md) - Memory buffer with metadata
- [`MemoryBufferWithRanges`](struct-MemoryBufferWithRanges.md) - Buffer with multiple ranges
- [`QueryMemoryPolicy`](enum-QueryMemoryPolicy.md) - Memory query policies

### Module Information
- [`Module`](struct-Module.md) - Module metadata
- [`ModuleInstance`](struct-ModuleInstance.md) - Module instance lifetime
- [`ModuleLoadedEvent`](struct-ModuleLoadedEvent.md) - Module load event
- [`ModuleUnloadedEvent`](struct-ModuleUnloadedEvent.md) - Module unload event

### Thread Information
- [`ThreadInfo`](struct-ThreadInfo.md) - Thread metadata
- [`ActiveThreadInfo`](struct-ActiveThreadInfo.md) - Active thread state
- [`ThreadCreatedEvent`](struct-ThreadCreatedEvent.md) - Thread creation event
- [`ThreadTerminatedEvent`](struct-ThreadTerminatedEvent.md) - Thread termination event
- [`UniqueThreadId`](type-UniqueThreadId.md) - Unique thread identifier

### Events and Breakpoints
- [`MemoryWatchpointData`](struct-MemoryWatchpointData.md) - Memory watchpoint information
- [`PositionWatchpointData`](struct-PositionWatchpointData.md) - Position watchpoint information
- [`ExceptionEvent`](struct-ExceptionEvent.md) - Exception event data
- [`GapData`](struct-GapData.md) - Execution gap event
- [`CustomEvent`](struct-CustomEvent.md) - Custom recording events

### System Information
- [`SystemInfo`](struct-SystemInfo.md) - System configuration
- [`TraceInfo`](struct-TraceInfo.md) - Trace metadata
- [`TraceFileInfo`](struct-TraceFileInfo.md) - Trace file information

## Enumerations

### Event Types
- [`EventType`](enum-EventType.md) - Types of trace events
- [`EventMask`](enum-EventMask.md) - Event filtering mask
- [`DataAccessType`](enum-DataAccessType.md) - Memory access types
- [`GapKind`](enum-GapKind.md) - Types of execution gaps
- [`GapEventType`](enum-GapEventType.md) - Gap event classifications

### Recording and Index
- [`RecordingType`](enum-RecordingType.md) - Recording method types
- [`IndexStatus`](enum-IndexStatus.md) - Index build status
- [`IndexBuildFlags`](enum-IndexBuildFlags.md) - Index building options
- [`TraceFileType`](enum-TraceFileType.md) - Trace file format types

### Configuration
- [`DebugModeType`](enum-DebugModeType.md) - Debug mode settings
- [`FindValidDataLineMode`](enum-FindValidDataLineMode.md) - Data line search modes

## Utility Classes and Templates

- [`Deleter`](template-Deleter.md) - RAII deletion helper for TTD interfaces
- [`Island`](struct-Island.md) - Execution island information
- [`Activity`](struct-Activity.md) - Recording activity data
- [`RecordClient`](struct-RecordClient.md) - Recording client information

## Type Aliases and Callbacks

- [`IndexBuildProgressCallback`](type-IndexBuildProgressCallback.md) - Progress reporting callback
- [`ExecutionCallback`](type-ExecutionCallback.md) - Execution event callback
- [`ClientErrorReportCallback`](type-ClientErrorReportCallback.md) - Error reporting callback (deprecated)

## Constants and Statistics

- [`IndexFileStats`](struct-IndexFileStats.md) - Index file statistics
- Various position constants (`Position::Invalid`, `Position::Min`, `Position::Max`)

## Debug Macros

This header requires debug assertion macros to be defined by the user:
- `DBG_ASSERT(cond)` - Basic assertion macro
- `DBG_ASSERT_MSG(cond, ...)` - Assertion with message and arguments

## Usage Pattern

```cpp
#include <TTD/IReplayEngine.h>
#include <TTD/IReplayEngineStl.h>

// Create replay engine using STL helpers
auto [engine, result] = TTD::Replay::MakeReplayEngine();
if (result == S_OK) {
    // Initialize with trace file
    if (engine->Initialize(L"trace.ttd")) {
        // Create cursor for navigation
        TTD::Replay::UniqueCursor cursor(engine->NewCursor());

        // Navigate timeline and query state
        auto lifetime = engine->GetLifetime();
        cursor->SetPosition(lifetime.Begin);

        // Query memory and registers
        auto registers = cursor->GetCrossPlatformContext();
        auto memory = cursor->QueryMemoryRange(someAddress);

        // Execute with breakpoints
        cursor->SetEventMask(EventMask::MemoryWatchpoint | EventMask::Exception);
        cursor->ExecuteForward();
    }
}
```

The replay engine provides powerful capabilities for analyzing recorded executions, supporting both forward and backward navigation through time with full access to program state at any point.
