# TTD::ILiveRecorder::DumpHeaps method

Method of the [`TTD::ILiveRecorder`](interface-ILiveRecorder.md) interface.

Adds to the trace file all the data of the process heaps.
This includes all data allocated with C/C++ functions like `malloc` and `operator new`.
It also includes data allocated with Windows APIs like
[`HeapAlloc`](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapalloc)
but it excludes data allocated with Windows APIs like
[`LoadLibrary`](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibrarya),
[`VirtualAlloc`](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc) and
[`MapViewOfFile`](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-mapviewoffile).

```C++
void TTD::ILiveRecorder::DumpHeaps() noexcept;
```

## Correct use

This method operates similarly to [`DumpSnapshot`](ILiveRecorder_DumpSnapshot.md) as called with `synchronous` set to `true`.
It may be freely called from multiple threads simultaneously, it protects itself against possible simultaneous de-commit
and may record "torn" data if it's simultaneously written. See [`DumpSnapshot`](ILiveRecorder_DumpSnapshot.md) for further details.

This method has no use restrictions.

Note that this method might record a large amount of data in the trace file.
TTD doesn't perform any differential or incremental snapshotting,
so the entirety of the process heaps will be recorded each time the mnethod is called.

## Example of use

```C++
// Record all the heaps before starting the recording, so it can all be seen in the debugger when debugging the trace file.
pRecorder->DumpHeaps();

pRecorder->StartRecordingCurrentThread(MyActivity, TTD::InstructionCount::Invalid);

// Now we're recording.
// All the heap allocated memory will be correct when debugging the trace file, even data that is not used here.
```
