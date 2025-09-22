# ExceptionEvent Structure

Exact definition from `IReplayEngine.h` (subset shown if structure extended in future versions):

```cpp
struct ExceptionEvent
{
	Position         Position;
	ThreadInfo const* pThreadInfo;
	uint32_t         Code;
	uint32_t         Flags;
};
```

## Members
- `Position Position`
- `ThreadInfo const* pThreadInfo`
- `uint32_t Code`
- `uint32_t Flags`

## See also
- [ThreadInfo](struct-ThreadInfo.md)
- [EventType](enum-EventType.md)

