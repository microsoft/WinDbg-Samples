# Position Structure

128-bit number that uniquely identifies a single instruction executed in the trace's timeline. Positions are the fundamental unit for navigating through TTD traces.

## Definition

```cpp
struct Position
{
    SequenceId Sequence = SequenceId::Invalid;
    StepCount  Steps    = StepCount::Zero;

    constexpr bool IsValid() const;
    Position& operator+=(int64_t increment);
    Position& operator+=(StepCount increment);

    static Position const Invalid;
    static Position const Min;
    static Position const Max;
};
```

## Members

### Sequence
- **Type**: `SequenceId`
- **Description**: Identifies the sequence or recording segment
- **Default**: `SequenceId::Invalid`

### Steps
- **Type**: `StepCount`  
- **Description**: Number of execution steps within the sequence
- **Default**: `StepCount::Zero`

## Static Constants

- `Position::Invalid` - Invalid position marker
- `Position::Min` - Minimum valid position
- `Position::Max` - Maximum valid position

## Methods

### IsValid()
```cpp
constexpr bool IsValid() const;
```
Returns `true` if the position represents a valid timeline location (i.e., `Sequence != SequenceId::Invalid`).

### operator+=
```cpp
Position& operator+=(int64_t increment);
Position& operator+=(StepCount increment);
```
Advances the position by the specified number of steps.

## Comparison Operations

Positions support full comparison operations (`==`, `!=`, `<`, `>`, `<=`, `>=`) and are ordered by sequence first, then by steps within the sequence.

```cpp
Position pos1{SequenceId{100}, StepCount{50}};
Position pos2{SequenceId{100}, StepCount{75}};
assert(pos1 < pos2); // Same sequence, pos1 has fewer steps

Position pos3{SequenceId{200}, StepCount{10}};
assert(pos2 < pos3); // Different sequences, sequence 100 < 200
```

## Usage

### Basic Position Operations
```cpp
// Create positions
Position start{SequenceId{1}, StepCount{0}};
Position current = start;

// Advance position
current += 100; // Advance by 100 steps

// Check validity
if (current.IsValid()) {
    // Use position for cursor navigation
    cursor->SetPosition(current);
}
```

### Timeline Navigation
```cpp
// Get trace lifetime
auto lifetime = engine->GetLifetime();
Position begin = lifetime.Begin;
Position end = lifetime.End;

// Navigate to middle of trace
StepCount totalSteps = end.Steps - begin.Steps;
Position middle = begin;
middle += totalSteps / 2;

cursor->SetPosition(middle);
```

### Position Ranges
```cpp
// Define a range of positions
PositionRange range{begin, end};

// Check if position is in range
if (current >= range.Begin && current <= range.End) {
    // Position is within range
    ProcessPosition(current);
}
```

## Important Notes

- Positions are monotonically increasing within a trace timeline
- Positions may have gaps - not every numeric value represents a valid position
- Use `IReplayEngine::GetNextPosition()` or `GetPreviousPosition()` to find valid adjacent positions
- Positions from different traces are not comparable
- Invalid positions should not be used for navigation operations

## See Also

- [`SequenceId`](../IdnaBasicTypes.h/enum-SequenceId.md) - Sequence identifier type
- [`StepCount`](enum-StepCount.md) - Step count type
- [`PositionRange`](struct-PositionRange.md) - Position range structure
- [`ICursorView::SetPosition`](interface-ICursorView.md#setposition) - Position navigation
