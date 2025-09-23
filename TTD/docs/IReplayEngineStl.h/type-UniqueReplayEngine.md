# TTD::Replay::UniqueReplayEngine Type Alias

Smart pointer type alias for managing `IReplayEngine` objects with automatic cleanup.

## Definition

```cpp
namespace TTD::Replay
{
    using UniqueReplayEngine = Unique<IReplayEngine>;
}
```

## Description

This type alias provides automatic memory management for `IReplayEngine` objects by wrapping them in a `std::unique_ptr` with the appropriate TTD deleter. This ensures that the replay engine is properly released when the smart pointer goes out of scope.

## Usage

```cpp
#include <TTD/IReplayEngineStl.h>

// Create engine using factory function
auto [engine, result] = TTD::Replay::MakeReplayEngine();
if (result == S_OK)
{
    // Use the engine
    result = engine->OpenTraceFile(traceFile);
    // ... other operations ...

    // Create cursor from engine
    TTD::Replay::UniqueCursor cursor;
    result = engine->NewCursor(cursor.GetAddressOf());
}
// engine automatically released when it goes out of scope
```

## Methods

As a `std::unique_ptr` wrapper, it provides standard smart pointer functionality:

- `get()` - Returns raw pointer
- `reset()` - Replaces managed object
- `release()` - Releases ownership
- `operator->()` - Access members of managed object
- `operator*()` - Dereference managed object
- `operator bool()` - Check if pointer is not null

## See Also

- [`TTD::Replay::MakeReplayEngine`](function-MakeReplayEngine.md) - Factory function that returns this type
- [`TTD::Replay::Unique`](type-Unique.md) - Base template for TTD smart pointers
- [`TTD::Replay::UniqueCursor`](type-UniqueCursor.md) - Smart pointer for cursor objects
