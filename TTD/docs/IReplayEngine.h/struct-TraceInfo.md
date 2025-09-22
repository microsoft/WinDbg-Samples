# TraceInfo Structure

Exact definition from `IReplayEngine.h`:

```cpp
struct TraceInfo
{
	PositionRange Lifetime;
	RecordingType RecordingType;
	uint32_t      ThreadCount;
};
```

## Members
- `PositionRange Lifetime`
- `RecordingType RecordingType`
- `uint32_t ThreadCount`

## See also
- [RecordingType](enum-RecordingType.md)
- [TraceFileInfo](struct-TraceFileInfo.md)

