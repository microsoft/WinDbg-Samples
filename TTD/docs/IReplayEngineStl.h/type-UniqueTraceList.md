# TTD::Replay Smart Pointer Type Aliases

Pre-defined type aliases for commonly used TTD interfaces wrapped in smart pointers.

## Type Aliases

### UniqueTraceList
```cpp
using UniqueTraceList = Unique<ITraceList>;
```
Smart pointer type for managing `ITraceList` objects with automatic cleanup.

### UniqueReplayEngine
```cpp
using UniqueReplayEngine = Unique<IReplayEngine>;
```
Smart pointer type for managing `IReplayEngine` objects with automatic cleanup.

### UniqueCursor
```cpp
using UniqueCursor = Unique<ICursor>;
```
Smart pointer type for managing `ICursor` objects with automatic cleanup.

## Usage

These aliases provide convenient, exception-safe management of TTD interfaces:

```cpp
#include <TTD/IReplayEngineStl.h>

// Create and automatically manage a replay engine
auto [engine, result] = TTD::Replay::MakeReplayEngine();
if (result == S_OK)
{
    // Create and manage a cursor
    TTD::Replay::UniqueCursor cursor;
    result = engine->NewCursor(cursor.GetAddressOf());

    if (result == S_OK)
    {
        // Use cursor - automatic cleanup when it goes out of scope
        cursor->SetPosition(somePosition);
        // ...
    }
    // cursor automatically released here
}
// engine automatically released here
```

## Benefits

- **Automatic cleanup**: Objects are automatically released when smart pointers go out of scope
- **Exception safety**: Resources are properly cleaned up even if exceptions occur
- **RAII compliance**: Follows Resource Acquisition Is Initialization principles
- **Move semantics**: Support for efficient transfer of ownership

These type aliases work seamlessly with the factory functions `MakeReplayEngine()` and `MakeTraceList()` which return the smart pointer types directly.
