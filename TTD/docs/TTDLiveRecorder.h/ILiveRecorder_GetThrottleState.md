# TTD::ILiveRecorder::GetThrottleState method

Method of the [`TTD::ILiveRecorder`](interface-ILiveRecorder.md) interface.

Allows the caller to retrieve the current island's progress towards the throttle, if any.

```C++
void TTD::ILiveRecorder::GetThrottleState(TTD::ThrottleState& throttleState) const noexcept;
```

## Parameters

| Name            | Type                                                                 | Semantic   | Description
|-                |-                                                                     |-           |-
| `throttleState` | [`TTD::ThrottleState&`](../TTDCommonTypes.h/struct-ThrottleState.md) | Out        | Returned object.

## Return value

On return, the current state of the throttle is written to the provided `throttleState` object.

`throttleState.InstructionsExecuted` always contains the recorded instruction count since the last
call to [`StartRecordingCurrentThread`](ILiveRecorder_StartRecordingCurrentThread.md)
or [`ResetThrottle`](ILiveRecorder_ResetThrottle.md), both of which reset the throttle.

If a throttle was specified in the current island via a call to [`StartRecordingCurrentThread`](ILiveRecorder_StartRecordingCurrentThread.md)
or [`ResetThrottle`](ILiveRecorder_ResetThrottle.md), `throttleState.InstructionsRemaining` contains
the number of instructions that will be recorded before reaching the throttle. If the throttle has already been reached,
then `throttleState.InstructionsRemaining` contains `TTD::InstructionCount::Zero`.

If the throttle was disabled (`TTD::InstructionCount::Invalid` was specified),
then `throttleState.InstructionsRemaining` contains `TTD::InstructionCount::Invalid`.

Note that any instruction counts returned by methods in this interface
will be already slightly stale by the time the method returns.
This is obvious if we consider that it takes instructions just to enter and exit a function or method.
Therefore such values should always be interpreted as approximate.

## Correct use

This method may be freely called from multiple threads simultaneously.

This method has no use restrictions.

## Convenience inlined overloads

Overloads and alternate methods are defined inlined in the header file. They are provided for convenience.

```C++
TTD::ThrottleState TTD::ILiveRecorder::GetThrottleState() const noexcept;
```

Provides the result as a return value instead of an "out" parameter.

## Example of use

```C++
TTD::ThrottleState throttleState = pRecorder->GetThrottleState();

if (throttleState.InstructionsRemaining == TTD::InstructionCount::Invalid)
{
    // There is no throttle.
}
else if (throttleState.InstructionsRemaining > TTD::InstructionCount::Zero)
{
    // The throttle had not yet engaged.
}
```
