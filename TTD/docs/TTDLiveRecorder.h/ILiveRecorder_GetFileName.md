# TTD::ILiveRecorder::GetFileName method

Method of the [`TTD::ILiveRecorder`](interface-ILiveRecorder.md) interface.

Allows the caller to retrieve the full path to the trace file that is currently being recorded.

```C++
size_t TTD::ILiveRecorder::GetFileName(
    wchar_t* pFileName,
    size_t   fileNameSize
) const noexcept;
```

## Parameters

| Name           | Type       | Semantic   | Description
|-               |-           |-           |-
| `pFileName`    | `wchar_t*` | Out buffer | Pointer to the output buffer, which must be at least `fileNameSize` characters in size.
| `fileNameSize` | `size_t`   | In         | Size of the `pFileName` in characters.

## Return value

On success, the `pFileName` buffer is filled with the null-terminated full path to the trace file and its length provided as the return value.

If no buffer is provided (`fileNameSize` is zero), returns zero.

If the path (including the string null terminator) is longer than `fileNameSize - 1`, a null terminator is written to `pFileName[0]` and the method returns zero.

## Correct use

This method may be freely called from multiple threads simultaneously.

This method has no use restrictions.

## Example of use

```C++
wchar_t traceFileName[MAX_PATH];
size_t size = pRecorder->GetFileName(traceFilename, MAX_PATH);
if (size > 0)
{
    printf("The trace file being recorded is %ls\n", traceFileName);
}
```
