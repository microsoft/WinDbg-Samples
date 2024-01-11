# TTD::ILiveRecorder::TryPauseRecording method

Method of the [`TTD::ILiveRecorder`](interface-ILiveRecorder.md) interface.

Resumes recording the current island after the recording was temporarily paused.

If for any reason the current thread is not recording this method does nothing and retrurns `false`.

```C++
bool TTD::ILiveRecorder::TryPauseRecording() noexcept;
```

## Return value

Returns `true` if the current thread was being recorded when the method was called.

## Correct use

This method only affects the recording of the calling thread and may be freely called from multiple threads simultaneously.

This method has no use restrictions.

After pausing the recording, it can be resumed by calling [`TTD::ILiveRecorder::ResumeRecording`](ILiveRecorder_ResumeRecording.md),
but this must only be done if this method returned `true`.

It is highly recommended to use the [`TTD::ILiveRecorder::ScopedPauseRecording`](ILiveRecorder_ScopedPauseRecording.md) class
instead of calling this method directly.

## Example of use

```C++
bool const wasRecording = pRecorder->TryPauseRecording();

// Recording is definitely paused now.
// We can do stuff that we don't want in the recording.

if (wasRecording)
{
    pRecorder->ResumeRecording();
}
```
