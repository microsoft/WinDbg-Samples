# `TTD::ThrottleState` struct

`struct` that provides outcome or current progress towards the throttle on a given thread.

## Fields

- `InstructionsExecuted` (`TTD::InstructionCount`) Specifies how many instructions have been recorded in the given thread's current island so far.
  Note that this value becomes ever so slightly older, or more stale, as the given thread continues to run.
- `InstructionsRemaining` (`TTD::InstructionCount`) Specifies how many instructions remain before the throttle engages.
  Note that if there's no throttle this will always specify `TTD::InstructionCount::Invalid`.

## Example of use

```C++
TTD::ThrottleState throttleState = pRecorder->GetThrottleState();

if (throttleState.InstructionsRemaining > TTD::InstructionCount::Zero)
{
    // The throttle had not yet engaged.
}
```
