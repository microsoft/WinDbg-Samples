# TTD::Replay::UniqueCursor Type Alias

Smart pointer type alias for managing `ICursor` objects with automatic cleanup.

## Definition

```cpp
namespace TTD::Replay
{
    using UniqueCursor = Unique<ICursor>;
}
```

## Description

This type alias provides automatic memory management for `ICursor` objects by wrapping them in a `std::unique_ptr` with the appropriate TTD deleter. Cursors are used to navigate through trace timelines and query execution state at specific positions.

## Usage

```cpp
#include <TTD/IReplayEngineStl.h>

// Assume we have a replay engine
TTD::Replay::UniqueReplayEngine engine = GetReplayEngine();

// Create a cursor
TTD::Replay::UniqueCursor cursor;
uint32_t result = engine->NewCursor(cursor.GetAddressOf());

if (result == S_OK)
{
    // Set position in the trace
    Position pos = GetSomePosition();
    cursor->SetPosition(pos);

    // Query state at this position
    uint32_t threadId;
    cursor->GetCurrentThreadId(threadId);

    // Read registers
    RegistersUnion registers;
    cursor->GetCrossPlatformContext(threadId, registers);
}
// cursor automatically released when it goes out of scope
```

## Methods

As a `std::unique_ptr` wrapper, it provides standard smart pointer functionality:

- `get()` - Returns raw pointer to the managed `ICursor`
- `reset()` - Replaces the managed cursor object
- `release()` - Releases ownership of the cursor
- `operator->()` - Access methods of the managed `ICursor`
- `operator*()` - Dereference to get the `ICursor` reference
- `operator bool()` - Check if the cursor pointer is not null

## Common Cursor Operations

Through the smart pointer, you can access all `ICursor` methods:

```cpp
cursor->SetPosition(position);
cursor->GetPosition(&currentPosition);
cursor->QueryMemory(address, buffer, bufferSize);
cursor->GetCurrentThreadId(threadId);
cursor->GetThreads(&threadList);
```

## See Also

- [`TTD::Replay::Unique`](type-Unique.md) - Base template for TTD smart pointers
- [`TTD::Replay::UniqueReplayEngine`](type-UniqueReplayEngine.md) - Smart pointer for replay engine objects
