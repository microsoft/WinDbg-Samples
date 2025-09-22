# TTD::Replay::Unique Type Alias

Template alias for `std::unique_ptr` that uses the appropriate TTD deleter for automatic memory management of TTD interfaces.

## Definition

```cpp
namespace TTD::Replay
{
    template < typename Interface >
    using Unique = std::unique_ptr<Interface, Deleter<Interface>>;
}
```

## Template Parameters

- `Interface` - The TTD interface type to manage

## Description

This template alias provides automatic memory management for TTD interfaces by wrapping them in a `std::unique_ptr` with the correct deleter. This ensures that TTD objects are properly released when they go out of scope.

## Usage

```cpp
// Instead of manually managing lifetime:
IReplayEngine* engine = nullptr;
CreateReplayEngine(engine, engineGuid);
// ... use engine ...
engine->Release(); // Manual cleanup required

// Use Unique for automatic management:
TTD::Replay::Unique<IReplayEngine> engine;
// engine automatically releases when it goes out of scope
```

## Related Type Aliases

The header provides convenient pre-defined aliases for common TTD interfaces:
- `UniqueTraceList` - `Unique<ITraceList>`
- `UniqueReplayEngine` - `Unique<IReplayEngine>`
- `UniqueCursor` - `Unique<ICursor>`

These aliases eliminate the need to specify the template parameter for the most commonly used interfaces.
