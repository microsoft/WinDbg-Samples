# TTD::ILiveRecorder::TryPauseRecording method

Method of the [`TTD::ILiveRecorder`](interface-ILiveRecorder.md) interface.

Temporarily pauses the recording of the current island, if any.

If the calling thread is recording, regardless of how the recording was started,
the recording is guaranteed to be paused upon return and this method returns `true`.
The recording of this thread will not resume until [`TTD::ILiveRecorder::ResumeRecording`](ILiveRecorder_ResumeRecording.md)
is called or a new island is started by calling [`TTD::ILiveRecorder::StartRecordingCurrentThread`](ILiveRecorder_StartRecordingCurrentThread.md).

If the calling thread is not recording this method does nothing and returns `false`.

```C++
bool TTD::ILiveRecorder::TryPauseRecording() noexcept;
```

## Return value

Returns `true` if the calling thread was being recorded when the method was called.

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

// Recording (if any) is definitely paused now.
// We can do stuff that we don't want in the recording.

if (wasRecording)
{
    pRecorder->ResumeRecording();
}
```
