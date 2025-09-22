# EventType Enumeration

Exact definition from `IReplayEngine.h`:

```cpp
enum class EventType : uint8_t
{
	MemoryWatchpoint  ,
	PositionWatchpoint,
	Exception   ,
	Gap         ,
	Thread      ,
	StepCount   ,
	Position    ,
	ModuleLoad  ,
	ModuleUnload,
	Custom      ,
};
```

Represents reasons execution stopped or events encountered while replaying.

## See also
- [EventMask](enum-EventMask.md)
- [GapData](struct-GapData.md)
- [ExceptionEvent](struct-ExceptionEvent.md)

