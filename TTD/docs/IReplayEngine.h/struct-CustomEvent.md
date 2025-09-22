# CustomEvent Structure

Exact definition from `IReplayEngine.h`:

```cpp
struct CustomEvent
{
	Position          Position;
	ThreadInfo const*  pThreadInfo;
	RecordClient const* pRecordClient;
	ConstBufferView   UserData;
};
```

## Members
- `Position Position`
- `ThreadInfo const* pThreadInfo`
- `RecordClient const* pRecordClient`
- `ConstBufferView UserData`

## See also
- [RecordClient](struct-RecordClient.md)
- [EventType](enum-EventType.md)

