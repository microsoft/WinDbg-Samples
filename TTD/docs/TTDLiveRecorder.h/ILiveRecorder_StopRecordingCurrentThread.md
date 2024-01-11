# TTD::ILiveRecorder::StopRecordingCurrentThread method

Method of the [`TTD::ILiveRecorder`](interface-ILiveRecorder.md) interface.

Stops recording the current island in the calling thread.

If the calling thread was not recording, then do nothing.

```C++
TTD::InstructionCount TTD::ILiveRecorder::StopRecordingCurrentThread() noexcept;
```

## Return value

Returns the count of instructions recorded so far in the current island, if any.
This is the same value that would have been returned by a call to [`GetThrottleState`](ILiveRecorder_GetThrottleState.md)
in the `throttleState.InstructionsExecuted` field.

Note that any instruction counts returned by methods in this interface
will be already slightly stale by the time the method returns.
This is obvious if we consider that it takes instructions just to enter and exit a function or method.
Therefore such values should always be interpreted as approximate.

## Correct use

This method only affects the recording of the calling thread and may be freely called from multiple threads simultaneously.

This method has no use restrictions.

## Example of use

```C++
// Record 100 more instructions and then stop.
TTD::InstructionCount instructionsRecorded = pRecorder->StopRecordingCurrentThread();
if (instructionsRecorded > TTD::InstructionCount{ 1'000'000 })
{
    // That was a large island, over a million instructions!
}
```
