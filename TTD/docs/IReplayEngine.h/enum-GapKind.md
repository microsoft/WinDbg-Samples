# GapKind Enumeration

Exact definition from `IReplayEngine.h`:

```cpp
enum class GapKind : uint8_t
{
	NoGap        , // There was a potential gap, but turned out not to be a gap (as in fallbacks).
	ContextSwitch, // No code was skipped in this thread, but potentially arbitrary amount of code run on other threads.
	Unrecorded   , // Some code on this thread was skipped, like a recorder internal pause, or a syscall.
	Large        , // Large gap between islands. The stack buffer may have been unrolled arbitrarily.
};

constexpr char const* GetGapKindName(_In_ GapKind const kind);

enum class GapKindMask : uint32_t;
constexpr GapKindMask ConvertGapKindToMask(_In_ GapKind const kind);
TTD_DEFINE_ENUM_FLAG_OPERATORS(GapKindMask)
```

`GapKindMask` supplies bitmask flags for filtering gap categories.

## See also
- [GapData](struct-GapData.md)
- [GapEventType](enum-GapEventType.md)

