# Deleter Template Class

Template class that provides proper cleanup for TTD interface objects by calling their `Destroy()` method. Used with smart pointers to ensure automatic resource management.

## Definition

```cpp
template<typename Interface>
struct Deleter
{
    void operator()(Interface* ptr) noexcept
    {
        if (ptr != nullptr)
        {
            ptr->Destroy();
        }
    }
};
```

## Template Parameters

- `Interface` - The TTD interface type to manage (e.g., `IReplayEngine`, `ICursor`, `ITraceList`)

## Usage

### With std::unique_ptr

The `Deleter` template is primarily used with `std::unique_ptr` to provide automatic cleanup:

```cpp
// Manual smart pointer usage
std::unique_ptr<IReplayEngine, Deleter<IReplayEngine>> engine;

// But prefer the predefined aliases from IReplayEngineStl.h:
TTD::Replay::UniqueReplayEngine engine;
TTD::Replay::UniqueCursor cursor;
TTD::Replay::UniqueTraceList traceList;
```

### Custom Deleter Function

```cpp
template<typename Interface>
void operator()(Interface* ptr) noexcept;
```

**Parameters:**
- `ptr` - Pointer to the TTD interface object to destroy

**Behavior:**
- Calls `ptr->Destroy()` if `ptr` is not null
- Does nothing if `ptr` is null
- Marked `noexcept` to ensure no exceptions are thrown during cleanup

## Integration with STL Smart Pointers

The deleter is designed to work seamlessly with STL smart pointers:

```cpp
#include <TTD/IReplayEngineStl.h>

// Factory functions return smart pointers with correct deleters
auto [engine, result] = TTD::Replay::MakeReplayEngine();
// engine is of type UniqueReplayEngine = std::unique_ptr<IReplayEngine, Deleter<IReplayEngine>>

auto [traceList, result2] = TTD::Replay::MakeTraceList();  
// traceList is of type UniqueTraceList = std::unique_ptr<ITraceList, Deleter<ITraceList>>
```

## Automatic Cleanup Examples

### RAII Scope Management
```cpp
void AnalyzeTrace(wchar_t const* filename)
{
    // Create engine - automatic cleanup on scope exit
    auto [engine, result] = TTD::Replay::MakeReplayEngine();
    if (result != S_OK) return;
    
    if (!engine->Initialize(filename)) return;
    
    // Create cursor - automatic cleanup on scope exit
    TTD::Replay::UniqueCursor cursor(engine->NewCursor());
    if (!cursor) return;
    
    // Use engine and cursor...
    cursor->SetPosition(somePosition);
    
    // Automatic cleanup occurs here when function exits
    // cursor->Destroy() called automatically
    // engine->Destroy() called automatically
}
```

### Exception Safety
```cpp
void ProcessTrace()
{
    auto [engine, result] = TTD::Replay::MakeReplayEngine();
    if (result == S_OK && engine->Initialize(L"trace.ttd")) {
        TTD::Replay::UniqueCursor cursor(engine->NewCursor());
        
        // Even if this throws an exception, cleanup is guaranteed
        ThrowingOperation();
        
        // Normal cleanup also guaranteed
    }
    // Resources automatically cleaned up regardless of how function exits
}
```

### Transfer of Ownership
```cpp
TTD::Replay::UniqueReplayEngine CreateConfiguredEngine(wchar_t const* filename)
{
    auto [engine, result] = TTD::Replay::MakeReplayEngine();
    if (result == S_OK && engine->Initialize(filename)) {
        // Configure engine...
        engine->BuildIndex(nullptr, nullptr);
        
        // Return ownership to caller
        return std::move(engine);
    }
    
    return nullptr; // Automatic cleanup of failed engine
}

void UseEngine()
{
    auto engine = CreateConfiguredEngine(L"trace.ttd");
    if (engine) {
        // Use the engine
        auto cursor = TTD::Replay::UniqueCursor(engine->NewCursor());
        // ...
    }
    // All resources automatically cleaned up
}
```

## Why Not Just Use delete?

TTD interfaces use a custom destruction pattern rather than standard C++ destructors:

```cpp
// DON'T DO THIS - undefined behavior
IReplayEngine* engine = CreateReplayEngine(...);
delete engine; // WRONG - will not properly clean up TTD internal state

// DO THIS - proper cleanup
engine->Destroy(); // Correct - calls TTD's cleanup code

// BETTER - use smart pointer with Deleter
TTD::Replay::UniqueReplayEngine engine = MakeReplayEngine().first;
// Automatic proper cleanup when smart pointer is destroyed
```

## Thread Safety

The `Deleter` template itself is thread-safe (it's stateless), but:
- The underlying TTD interfaces may not be thread-safe
- The `Destroy()` method should only be called once per object
- Don't share raw pointers across threads when using smart pointers

## See Also

- [`TTD::Replay::Unique`](../IReplayEngineStl.h/type-Unique.md) - Base template for TTD smart pointers
- [`TTD::Replay::UniqueReplayEngine`](../IReplayEngineStl.h/type-UniqueReplayEngine.md) - Smart pointer for replay engines  
- [`TTD::Replay::UniqueCursor`](../IReplayEngineStl.h/type-UniqueCursor.md) - Smart pointer for cursors
- [`TTD::Replay::MakeReplayEngine`](../IReplayEngineStl.h/function-MakeReplayEngine.md) - Factory function returning smart pointer
