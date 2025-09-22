# TraceFileInfo Structure

Exact definition from `IReplayEngine.h`:

```cpp
struct TraceFileInfo
{
	TraceFileType FileType;
	uint64_t      SizeBytes;
	bool          HasIndex;
};
```

## Members
- `TraceFileType FileType`
- `uint64_t SizeBytes`
- `bool HasIndex`

## See also
- [TraceFileType](enum-TraceFileType.md)
- [IndexStatus](enum-IndexStatus.md)

