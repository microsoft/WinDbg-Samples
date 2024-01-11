# TTD::ILiveRecorder::DumpSnapshot method

Method of the [`TTD::ILiveRecorder`](interface-ILiveRecorder.md) interface.

Adds to the trace file the contents of a memory buffer.
This memory buffer becomes visible as target process memory when using the trace file in a debugger.

```C++
void TTD::ILiveRecorder::DumpSnapshot(
    void const* pBeginAddress,
    void const* pEndAddress,
    bool        synchronous
) noexcept;
```

## Parameters

| Name            | Type          | Semantic | Description
|-                |-              |-         |-
| `pBeginAddress` | `void const*` | In       | Pointer to the beginning of the memory buffer.
| `pEndAddress`   | `void const*` | In       | Pointer just past the end of the memory buffer.
| `synchronous`   | `bool`        | In       | Don't return from this method until the buffer is completely recorded.

`pBeginAddress` and `pEndAddress` behave exactly like `begin` and `end` iterators in the C++ standard library.

Setting `synchronous` to false performs the buffer recording in the background, so this method returns immediately.

## Correct use

If buffer pointers don't denote a valid buffer, this method does nothing.

If `synchronous` is `true`, this method uses synchronization such that it may be freely called from multiple threads simultaneously.
If there is high contention, the method may take an arbitrarily amount of time to complete.

If `synchronous` is `false`, this method schedules the operation to happen in the background
and may be freely called from multiple threads simultaneously.

This method has no use restrictions.

This method handles the case where any portion of the buffer memory is de-committed while it's being copied,
without causing access violations or other hard faults, and without recording incorrect data.

However, this method constitutes a "read" of the data in the buffer and is subject to data races unless proper synchronization is used.
If the memory buffer is modified by some thread at the same time it's being recorded, "torn" data might end up in the recording,
where part of the data is before the modification and part is after the modification.

Note that if given a large buffer this method will record it in its entirety in the trace file.
TTD doesn't perform any differential or incremental snapshotting,
so the entirety of the buffer will be recorded each time the mnethod is called.

## Example of use

```C++
std::vector<int> databuffer;

// Load or construct some data in the data buffer, before we start recording.

pRecorder->DumpSnapshot(dataBuffer.data(), dataBuffer.data() + dataBuffer.size(), true);

pRecorder->StartRecordingCurrentThread(MyActivity, TTD::InstructionCount::Invalid);

// Now we're recording. Run some code that uses some of the data in the buffer.
// The entire buffer will be visible when debugging the trace file, even data that is not used here.
```
