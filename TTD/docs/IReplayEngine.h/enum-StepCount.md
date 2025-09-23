# StepCount Enumeration

Represents the number of atomic execution steps within a sequence. Steps include instruction executions and other events that don't necessarily execute instructions (like exceptions or context updates).

## Definition

```cpp
enum class StepCount : uint64_t
{
    Zero    = 0,
    Min     = 0,
    Max     = uint64_t(-2),
    Invalid = uint64_t(-1),
};
```

## Constants

- `StepCount::Zero` - No steps (starting point)
- `StepCount::Min` - Minimum valid step count (same as Zero)
- `StepCount::Max` - Maximum valid step count
- `StepCount::Invalid` - Invalid step count marker

## Arithmetic Operations

StepCount supports various arithmetic operations for navigation and calculation:

### Addition and Subtraction
```cpp
constexpr StepCount operator+(StepCount a, uint64_t b);
constexpr StepCount operator-(StepCount a, uint64_t b);
constexpr StepCount operator+(StepCount a, StepCount b);
constexpr StepCount operator-(StepCount a, StepCount b);

StepCount& operator+=(StepCount& a, int64_t b);
StepCount& operator-=(StepCount& a, int64_t b);
StepCount& operator+=(StepCount& a, StepCount b);
StepCount& operator-=(StepCount& a, StepCount b);
```

### Division and Modulo
```cpp
constexpr StepCount operator/(StepCount a, uint64_t b);
constexpr uint64_t  operator%(StepCount a, uint64_t b);
```

### Comparison Operations
```cpp
constexpr bool operator==(StepCount a, uint64_t b);
constexpr bool operator!=(StepCount a, uint64_t b);
constexpr bool operator< (StepCount a, uint64_t b);
constexpr bool operator> (StepCount a, uint64_t b);
constexpr bool operator<=(StepCount a, uint64_t b);
constexpr bool operator>=(StepCount a, uint64_t b);
```

## Relationship with InstructionCount

StepCount can be mixed with `InstructionCount` in limited ways, since instructions are a subset of steps:

```cpp
constexpr StepCount operator+(StepCount a, InstructionCount b);
constexpr StepCount operator-(StepCount a, InstructionCount b);
constexpr StepCount operator+(InstructionCount a, StepCount b);

// Comparison operations
constexpr bool operator==(StepCount a, InstructionCount b);
constexpr bool operator< (StepCount a, InstructionCount b);
// ... etc
```

## Conversion Functions

### ToStepCount()
```cpp
constexpr StepCount ToStepCount(InstructionCount const count);
```
Converts an instruction count to a step count.

```cpp
InstructionCount instructions{1000};
StepCount steps = ToStepCount(instructions);
```

## Important Notes

- Steps are more granular than instructions - multiple events can occur at the same instruction
- Not all numeric values represent valid step counts in a trace
- Step counts are only meaningful within the same sequence
- Use `StepCount::Invalid` to indicate error conditions
- Step arithmetic should account for potential overflow near `StepCount::Max`

## See Also

- [`Position`](struct-Position.md) - Uses StepCount to identify timeline positions
- [`InstructionCount`](../TTDCommonTypes.h/type-InstructionCount.md) - Instruction-specific counting
- [`SequenceId`](../IdnaBasicTypes.h/enum-SequenceId.md) - Sequence identification
