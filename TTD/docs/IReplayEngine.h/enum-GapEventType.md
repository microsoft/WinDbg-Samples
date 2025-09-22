# GapEventType Enumeration

Exact definition from `IReplayEngine.h`:

```cpp
enum GapEventType : uint8_t
{
	SyntheticSequence        , // is just inserting a sequence to end the current fragment.
	CodeCacheFlush           , // needs to flush its code cache to pick up modified code.
	PreAtomicOperation       , // is about to do an atomic operation.
	PotentialAtomicCollision , // just finished an atomic operation. TODO: faithful ordering of atomics.
	EtwEvent                 , // is recording an ETW event.
	DebugBreak               , // encountered, and skipped, a hardcoded breakpoint.
	FastFail                 , // encountered a fastfail. The process is about to end.
	KernelCall               , // called into kernel.
	SyntheticFallback        , // encountered a synthetic fallback.
	ExceptionDispatch        , // jumped to dispatch an exception.
	UnknownInstruction       , // emulator couldn't handle the instruction.
	ThreadSuspended          , // was suspended.
	SListRollback            , // A-B-A exception in SList pop; rolled back.
	SyncPoint                , // was stopped in the debugger.
	PauseEmulation           , // paused emulation without accruing an instruction.
	StopEmulation            , // stopped emulation to end an island.
	Throttled                , // stopped because of a throttle.
};

constexpr char const* GetGapEventTypeName(_In_ GapEventType const kind);

enum class GapEventMask : uint32_t;
constexpr GapEventMask ConvertGapEventTypeToMask(_In_ GapEventType const kind);
TTD_DEFINE_ENUM_FLAG_OPERATORS(GapEventMask)
```

## See also
- [GapData](struct-GapData.md)
- [GapKind](enum-GapKind.md)

