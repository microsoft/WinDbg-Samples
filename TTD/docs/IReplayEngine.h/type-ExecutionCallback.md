# ExecutionCallback Type

Callback invoked on execution events during `ICursor` runs.

## Typical signature
```cpp
using ExecutionCallback = void(*)(EventType type, void const* eventData);
```

## See also
- [EventType](enum-EventType.md)
- [ICursor](interface-ICursor.md)
