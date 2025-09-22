# ThreadId Enumeration

Represents OS thread identifiers within the TTD recording and replay system. Thread IDs come from the OS and are not guaranteed to be unique (values may be recycled by the OS).

## Definition

```cpp
enum class ThreadId : uint32_t
{
    // Thread IDs are ordered, so provide explicit valid min/max.
    Min     = 1,
    Max     = UINT32_MAX - 1,

    Invalid = 0,
    Any     = UINT32_MAX,
};
```

## Values

- `ThreadId::Invalid` (0) - Invalid or uninitialized thread identifier
- `ThreadId::Min` (1) - Minimum valid thread identifier
- `ThreadId::Max` (UINT32_MAX - 1) - Maximum valid thread identifier
- `ThreadId::Any` (UINT32_MAX) - Special identifier representing any/all threads

## Important Notes

- Thread ID 0 is reserved for invalid/uninitialized states
- Thread ID UINT32_MAX is reserved for operations affecting all threads
- Use `ThreadId::Any` for operations that should apply to all threads
- Consider thread lifecycle when maintaining thread-specific data
- Thread synchronization events can be tracked for analysis

## See Also

- [`SequenceId`](enum-SequenceId.md) - Timeline sequence identification
- [`Position`](../IReplayEngine.h/struct-Position.md) - Timeline positions that can be thread-specific
- [`ThreadRecordingState`](../TTDCommonTypes.h/enum-ThreadRecordingState.md) - Thread recording state enumeration
