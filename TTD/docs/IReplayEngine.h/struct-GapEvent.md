# GapEvent Structure

Represents a period where no instructions from the target were executed, typically due to waits or scheduling gaps.

## Typical members
- Kind: GapKind — category of the gap
- Type: GapEventType — classification of the gap reason
- Duration: uint64_t — approximate duration (if provided)

## See also
- [GapKind](enum-GapKind.md)
- [GapEventType](enum-GapEventType.md)
