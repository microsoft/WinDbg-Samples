# TTD::Replay::MakeReplayEngine Function

Convenience factory function that creates a replay engine and returns it wrapped in a smart pointer for automatic memory management.

## Definition

```cpp
inline std::pair<UniqueReplayEngine, uint32_t> MakeReplayEngine(
    _In_ GUID const& engineGuid = __uuidof(IReplayEngineView)
) noexcept;
```

## Parameters

- `engineGuid` - GUID identifying the engine type to create (defaults to `__uuidof(IReplayEngineView)`)

## Return Value

A `std::pair` containing:
- `UniqueReplayEngine` - Smart pointer to the created replay engine (null if creation failed)
- `uint32_t` - Result code (`S_OK` on success, error HRESULT on failure)

## Behavior

The function:
1. Calls `CreateReplayEngine` internally with the provided GUID
2. Wraps the resulting raw pointer in a `UniqueReplayEngine` smart pointer
3. Returns both the smart pointer and the result code

## Usage

### Basic Usage
```cpp
auto [engine, result] = TTD::Replay::MakeReplayEngine();
if (result == S_OK)
{
    // Engine created successfully, use it
    result = engine->OpenTraceFile(L"mytrace.ttd");
    
    if (result == S_OK)
    {
        TTD::Replay::UniqueCursor cursor;
        result = engine->NewCursor(cursor.GetAddressOf());
        // ... use cursor ...
    }
}
// engine automatically cleaned up when it goes out of scope
```

### With Custom Engine GUID
```cpp
GUID customEngineGuid = { /* custom GUID */ };
auto [engine, result] = TTD::Replay::MakeReplayEngine(customEngineGuid);
```

### Error Handling
```cpp
auto [engine, result] = TTD::Replay::MakeReplayEngine();
if (result != S_OK)
{
    printf("Failed to create replay engine: 0x%x\n", result);
    return;
}

// Safe to use engine here
assert(engine.get() != nullptr);
```

## Benefits

- **Automatic cleanup**: The returned smart pointer automatically releases the engine when destroyed
- **Exception safety**: No memory leaks even if exceptions occur after creation
- **Convenient error handling**: Both the object and result code are returned together
- **RAII compliance**: Follows Resource Acquisition Is Initialization principles

This function is preferred over `CreateReplayEngine` when working with modern C++ code that uses smart pointers and RAII patterns.
