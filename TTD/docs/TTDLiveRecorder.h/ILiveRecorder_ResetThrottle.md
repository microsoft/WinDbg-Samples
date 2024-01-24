# TTD::ILiveRecorder::ResetThrottle method

Method of the [`TTD::ILiveRecorder`](interface-ILiveRecorder.md) interface.

Sets a new instruction count to use as a throttle.

```C++
TTD::InstructionCount TTD::ILiveRecorder::ResetThrottle(
    TTD::InstructionCount maxInstructionsToRecord
) noexcept;
```

## Parameters

| Name                      | Type                                                                    | Semantic | Description
|-                          |-                                                                        |-         |-
| `maxInstructionsToRecord` | [`TTD::InstructionCount`](../TTDCommonTypes.h/type-InstructionCount.md) | In       | New instruction count to use as a throttle.

If `maxInstructionsToRecord` is set to `TTD::InstructionCount::Invalid` then there will be no throttle
and recording will proceed until it's stopped by other means.

## Return value

Returns the count of instructions recorded so far in the island.
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
TTD::InstructionCount instructionsSoFar = pRecorder->ResetThrottle(TTD::InstructionCount{ 100 });
```
