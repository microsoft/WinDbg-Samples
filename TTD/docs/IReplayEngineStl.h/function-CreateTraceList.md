# TTD::Replay::CreateTraceList Function

Factory function that creates a trace list instance for managing multiple trace files.

## Definition

```cpp
extern "C" uint32_t __cdecl CreateTraceList(
    _Out_ ITraceList*&         pTraceList,
    _In_  GUID          const& engineGuid
) noexcept;
```

## Parameters

- `pTraceList` - Output parameter that receives a pointer to the created `ITraceList` interface
- `engineGuid` - GUID identifying the trace list implementation to create

## Return Value

- `S_OK` (0) on success
- Error HRESULT on failure

## Behavior

The function creates an instance of the trace list interface specified by the GUID. The caller is responsible for releasing the returned interface when done.

## Usage

```cpp
ITraceList* traceList = nullptr;
uint32_t result = TTD::Replay::CreateTraceList(traceList, __uuidof(ITraceListView));

if (result == S_OK)
{
    // Use the trace list
    result = traceList->AddTrace(L"trace1.ttd");
    result = traceList->AddTrace(L"trace2.ttd");

    // Manual cleanup required
    traceList->Release();
}
```

## Memory Management

This function returns a raw interface pointer that must be manually released by calling `Release()`. For automatic memory management, consider using [`MakeTraceList`](function-MakeTraceList.md) instead, which returns a smart pointer.

## See Also

- [`TTD::Replay::MakeTraceList`](function-MakeTraceList.md) - Smart pointer version of this function
- [`TTD::Replay::UniqueTraceList`](type-UniqueTraceList.md) - Smart pointer type for trace lists
