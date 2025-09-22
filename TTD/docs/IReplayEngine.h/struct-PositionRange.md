# PositionRange Structure

Represents a half-open timeline range [Begin, End). Typically used to scope analysis to a subset of the trace.

## Members
- Begin: Position — inclusive start position
- End: Position — exclusive end position

## Usage
```cpp
PositionRange r{ beginPos, endPos };

bool Contains(Position p) {
    return p >= r.Begin && p < r.End;
}
```

## Notes
- Half-open ranges avoid off-by-one errors and compose cleanly.

## See also
- [Position](struct-Position.md)
- [StepCount](enum-StepCount.md)
