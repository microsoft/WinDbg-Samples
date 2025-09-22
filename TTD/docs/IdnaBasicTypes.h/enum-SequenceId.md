# SequenceId Enumeration

Represents a sequence identifier used to organize and order timeline data within TTD traces. Sequences provide a high-level partitioning of the execution timeline and ensure proper
causality between threads.

## Definition

```cpp
enum class SequenceId : uint64_t
{
    // Sequence IDs are ordered, so provide explicit valid min/max.
    Min     = 0,
    Max     = UINT64_MAX - 1,

    Invalid = UINT64_MAX,
};
```

## Values

- `SequenceId::Min` (0) - Minimum valid sequence identifier
- `SequenceId::Max` (UINT64_MAX - 1) - Maximum valid sequence identifier  
- `SequenceId::Invalid` (UINT64_MAX) - Invalid sequence marker

## Arithmetic Operations

SequenceId supports arithmetic operations for sequence manipulation:

### Addition and Subtraction
```cpp
__forceinline SequenceId  operator+ (SequenceId addr, int64_t value);
__forceinline SequenceId  operator- (SequenceId addr, int64_t value);
__forceinline SequenceId& operator+=(SequenceId& addr, int64_t value);
__forceinline SequenceId& operator-=(SequenceId& addr, int64_t value);
```

### Distance Calculation
```cpp
__forceinline int64_t operator-(SequenceId addr1, SequenceId addr2);
```
Calculates the signed distance between two sequence identifiers.

## Important Notes

- Sequence IDs are ordered and can be compared directly
- The range [Min, Max] represents all possible valid sequences
- Invalid sequences should be handled gracefully in applications
- Sequence arithmetic can overflow, so bounds checking may be necessary
- Sequences provide high-level timeline organization - finer granularity is handled by step counts within positions

## See Also

- [`Position`](../IReplayEngine.h/struct-Position.md) - Uses SequenceId for timeline positioning
- [`StepCount`](../IReplayEngine.h/enum-StepCount.md) - Fine-grained steps within sequences
