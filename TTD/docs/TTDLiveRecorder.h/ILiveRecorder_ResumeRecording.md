# TTD::ILiveRecorder::ResumeRecording method

Method of the [`TTD::ILiveRecorder`](interface-ILiveRecorder.md) interface.

Resumes recording the current island after the recording was temporarily paused.

If the recording is not paused (either because the calling thread is recording or because it never started recording
or because recording was stopped) this method does nothing.

```C++
bool TTD::ILiveRecorder::ResumeRecording() noexcept;
```

## Return value

Returns `true` if the calling thread is being recorded when the method returns.

## Correct use

This method only affects the recording of the calling thread and may be freely called from multiple threads simultaneously.

This method must only be called after a call to [`TTD::ILiveRecorder::TryPauseRecording`](ILiveRecorder_TryPauseRecording.md)
that returned `true`, signifying a successful pause of the recording.

It is highly recommended to use the [`TTD::ILiveRecorder::ScopedPauseRecording`](ILiveRecorder_ScopedPauseRecording.md) class
instead of calling this method directly.

## Example of use

```C++
bool const wasRecording = pRecorder->TryPauseRecording();

// Recording (if any) is definitely paused now.
// We can do stuff that we don't want in the recording.

if (wasRecording)
{
    pRecorder->ResumeRecording();
}
```
