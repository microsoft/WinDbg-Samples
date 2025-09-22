# FindValidDataLineMode Enumeration

Exact definition:

```cpp
enum class FindValidDataLineMode : uint32_t
{
	IncludePreviouslyReturnedDataLines,
	ExcludePreviouslyReturnedDataLines
};
```

Controls whether already-returned data lines may be returned again.

## Enumerators
- `IncludePreviouslyReturnedDataLines` – Allow duplicates.
- `ExcludePreviouslyReturnedDataLines` – Suppress duplicates.

## See also
- [DebugModeType](enum-DebugModeType.md)

