# `TTD::ThreadRecordingState` enumeration

8-bit enumeration type that indicates the current state of TTD's recorder on a thread.

It is implemented as a C++ `enum class` to make it a strong type.

## Values

- `TTD::ThreadRecordingState::NotStarted` (0) Indicates that the current thread is not currently being recorded,
  either because it has never been recording, or because recording has been stopped.
- `TTD::ThreadRecordingState::Recording` (1) Indicates that the current thread is currently being recorded.
- `TTD::ThreadRecordingState::Paused` (2) Indicates that the current thread was being recorded but
  the recording was temporarily paused.
- `TTD::ThreadRecordingState::Throttled` (3) Indicates that the current thread was being recorded but
  the recording was stopped by the throttle.

## Supported operations

As a strongly typed enumeration value, `TTD::ThreadRecordingState` supports defaulted comparisons:

- Comparison `==`, `!=`, `<`, `>`, `<=`, `>=` with another `TTD::ThreadRecordingState`.

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
