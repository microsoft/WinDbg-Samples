# TTD::Replay::MakeTraceList Function

Convenience factory function that creates a trace list and returns it wrapped in a smart pointer for automatic memory management.

## Definition

```cpp
inline std::pair<UniqueTraceList, uint32_t> MakeTraceList(
    _In_ GUID const& engineGuid = __uuidof(ITraceListView)
) noexcept;
```

## Parameters

- `engineGuid` - GUID identifying the trace list implementation to create (defaults to `__uuidof(ITraceListView)`)

## Return Value

A `std::pair` containing:
- `UniqueTraceList` - Smart pointer to the created trace list (null if creation failed)
- `uint32_t` - Result code (`S_OK` on success, error HRESULT on failure)

## Behavior

The function:
1. Calls `CreateTraceList` internally with the provided GUID
2. Wraps the resulting raw pointer in a `UniqueTraceList` smart pointer
3. Returns both the smart pointer and the result code

## Usage

### Basic Usage
```cpp
auto [traceList, result] = TTD::Replay::MakeTraceList();
if (result == S_OK)
{
    // Add multiple trace files to the list
    result = traceList->AddTrace(L"trace1.ttd");
    result = traceList->AddTrace(L"trace2.ttd");
    result = traceList->AddTrace(L"trace3.ttd");
    
    // Get information about the traces
    uint32_t traceCount;
    result = traceList->GetTraceCount(&traceCount);
    
    for (uint32_t i = 0; i < traceCount; ++i)
    {
        wchar_t* tracePath;
        result = traceList->GetTracePath(i, &tracePath);
        if (result == S_OK)
        {
            wprintf(L"Trace %u: %s\n", i, tracePath);
            // ... process trace ...
        }
    }
}
// traceList automatically cleaned up when it goes out of scope
```

### Error Handling
```cpp
auto [traceList, result] = TTD::Replay::MakeTraceList();
if (result != S_OK)
{
    printf("Failed to create trace list: 0x%x\n", result);
    return;
}

// Safe to use traceList here
assert(traceList.get() != nullptr);
```

### With Custom GUID
```cpp
GUID customGuid = { /* custom trace list GUID */ };
auto [traceList, result] = TTD::Replay::MakeTraceList(customGuid);
```

## Benefits

- **Automatic cleanup**: The returned smart pointer automatically releases the trace list when destroyed
- **Exception safety**: No memory leaks even if exceptions occur after creation
- **Convenient error handling**: Both the object and result code are returned together
- **RAII compliance**: Follows Resource Acquisition Is Initialization principles

## Use Cases

Trace lists are useful when:
- Working with multiple related trace files
- Batch processing multiple traces
- Analyzing traces collected from different runs or time periods
- Managing trace files as a collection

This function is preferred over `CreateTraceList` when working with modern C++ code that uses smart pointers and RAII patterns.
