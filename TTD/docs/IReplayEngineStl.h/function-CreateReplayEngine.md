# TTD::Replay Factory Functions

Factory functions for creating TTD objects with convenient STL integration.

## CreateReplayEngine

```cpp
extern "C" uint32_t __cdecl CreateReplayEngine(
    _Out_ IReplayEngine*&      pReplayEngine,
    _In_  GUID          const& engineGuid
) noexcept;
```

Creates a replay engine instance.

**Parameters:**
- `pReplayEngine` - Output parameter that receives the created engine interface
- `engineGuid` - GUID identifying the engine type to create

**Return Value:**
- `S_OK` on success
- Error HRESULT on failure

## MakeReplayEngine

```cpp
inline std::pair<UniqueReplayEngine, uint32_t> MakeReplayEngine(
    _In_ GUID const& engineGuid = __uuidof(IReplayEngineView)
) noexcept;
```

Convenience function that creates a replay engine and returns it wrapped in a smart pointer.

**Parameters:**
- `engineGuid` - GUID identifying the engine type (defaults to `IReplayEngineView`)

**Return Value:**
A `std::pair` containing:
- `UniqueReplayEngine` - Smart pointer to the created engine (null on failure)
- `uint32_t` - Result code (`S_OK` on success)

**Usage:**
```cpp
auto [engine, result] = TTD::Replay::MakeReplayEngine();
if (result == S_OK)
{
    // Use engine - automatic cleanup when it goes out of scope
    engine->OpenTraceFile(L"trace.ttd");
}
```

## CreateTraceList

```cpp
extern "C" uint32_t __cdecl CreateTraceList(
    _Out_ ITraceList*&         pTraceList,
    _In_  GUID          const& engineGuid
) noexcept;
```

Creates a trace list instance for managing multiple trace files.

**Parameters:**
- `pTraceList` - Output parameter that receives the created trace list interface  
- `engineGuid` - GUID identifying the trace list type to create

**Return Value:**
- `S_OK` on success
- Error HRESULT on failure

## MakeTraceList

```cpp
inline std::pair<UniqueTraceList, uint32_t> MakeTraceList(
    _In_ GUID const& engineGuid = __uuidof(ITraceListView)
) noexcept;
```

Convenience function that creates a trace list and returns it wrapped in a smart pointer.

**Parameters:**
- `engineGuid` - GUID identifying the trace list type (defaults to `ITraceListView`)

**Return Value:**
A `std::pair` containing:
- `UniqueTraceList` - Smart pointer to the created trace list (null on failure)
- `uint32_t` - Result code (`S_OK` on success)

**Usage:**
```cpp
auto [traceList, result] = TTD::Replay::MakeTraceList();
if (result == S_OK)
{
    // Use trace list for managing multiple traces
    traceList->AddTrace(L"trace1.ttd");
    traceList->AddTrace(L"trace2.ttd");
}
```

The `Make*` functions provide exception-safe, RAII-compliant object creation with automatic memory management through smart pointers.
