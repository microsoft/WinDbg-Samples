# TTD::ILiveRecorder::GetState method

Method of the [`TTD::ILiveRecorder`](interface-ILiveRecorder.md) interface.

Allows the caller to know the current state of TTD's recorder on the calling thread.

```C++
TTD::ThreadRecordingState TTD::ILiveRecorder::GetState() const noexcept;
```

## Return value

Returns the value of the [`TTD::ThreadRecordingState`](../TTDCommonTypes.h/enum-ThreadRecordingState.md) enumeration
that best represents the current state of the recorder on the calling thread.

Note that the returned state may become stale by the time this method returns or shortly thereafter.
For instance, this method might return `TTD::ThreadRecordingState::Recording` and then
immediately get throttled before the calling code has any time to react.
Therefore this state should always be interpreted as "it was accurate recently".

## Correct use

This method may be freely called from multiple threads simultaneously.

This method has no use restrictions.

## Example of use

```C++
switch (pRecorder->GetState())
{
case TTD::ThreadRecordingState::NotStarted:
    // We're not being recorded
    break;
case TTD::ThreadRecordingState::Recording:
    // We're being recorded
    break;
case TTD::ThreadRecordingState::Paused:
    // Recording is paused
    break;
case TTD::ThreadRecordingState::Throttled:
    // Recording was stopped by the throttle
    break;
}
```
