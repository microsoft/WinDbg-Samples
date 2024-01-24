# TTD::ILiveRecorder::DumpModuleData method

Method of the [`TTD::ILiveRecorder`](interface-ILiveRecorder.md) interface.

Adds to the trace file some or all of the memory of a loaded module (executable or DLL).

```C++
void TTD::ILiveRecorder::DumpModuleData(
    HMODULE module,
    bool    writableOnly
) noexcept;
```

## Parameters

| Name           | Type          | Semantic | Description
|-               |-              |-         |-
| `module`       | `void const*` | In       | Handle to the module that will be written to the trace file.
| `writableOnly` | `bool`        | In       | Allows only the writable data sections to be written.

`module` is a handle as can be obtained in Windows via the
[`LoadLibrary`](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibrarya) or
[`GetModuleHandle`](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulehandlea)
APIs.


## Correct use

If `module` doesn't denote a currently-loaded module, this method does nothing.

This method operates similarly to [`DumpSnapshot`](ILiveRecorder_DumpSnapshot.md) as called with `synchronous` set to `true`.
It may be freely called from multiple threads simultaneously, it protects itself against possible simultaneous de-commit
and may record "torn" data if it's simultaneously written. See [`DumpSnapshot`](ILiveRecorder_DumpSnapshot.md) for further details.

This method has no use restrictions.

Also note that in automatic mode [`TTD.exe -recordMode Automatic`](https://learn.microsoft.com/en-us/windows-hardware/drivers/debuggercmds/time-travel-debugging-ttd-exe-command-line-util#reducing-overhead-of-tracing), which is TTD's default,
the recorder will already have recorded all the memory of the process as part of its initialization,
and any additional modules as soon as they are loaded, so calling this method might be redundant and wasteful.
Caution and common sense should be exercised to avoid bloating the trace file with large blocks of redundant data.

## Example of use

```C++
// Record all the code and data in MathLibrary.dll, so it can be seen in the debugger when debugging the trace file.
pRecorder->DumpModuleData(GetModuleHandleA("MathLibrary.dll", false);

// Use the math library without recording.

// Re-record the writable data segment of MathLibrary.dll, which may have been modified after it was initially recorded.
pRecorder->DumpModuleData(GetModuleHandleA("MathLibrary.dll", true);

pRecorder->StartRecordingCurrentThread(MyActivity, TTD::InstructionCount::Invalid);

// Now we're recording. Run some code that uses the math library.
// The library's global data will be correct when debugging the trace file, even data that is not used here.
```
