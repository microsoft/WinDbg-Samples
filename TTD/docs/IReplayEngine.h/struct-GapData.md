# GapData Structure

Exact definition from `IReplayEngine.h`:

```cpp
struct GapData
{
    GapKind      Kind;
    GapEventType Event;
};
```

Represents an execution interruption (gap). `Kind` gives the broad category; `Event` gives the specific reason.

## Members
- `GapKind Kind` – High-level gap category.
- `GapEventType Event` – Specific event classification within the gap.

## See also
- [GapKind](enum-GapKind.md)
- [GapEventType](enum-GapEventType.md)
