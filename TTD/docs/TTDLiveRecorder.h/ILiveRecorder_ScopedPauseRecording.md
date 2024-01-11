# TTD::ILiveRecorder::ScopedPauseRecording class

Subclass of the [`TTD::ILiveRecorder`](interface-ILiveRecorder.md) interface.

[RAII](https://en.cppreference.com/w/cpp/language/raii) class to safely and correctly pause the recorder,
to run a block of code that the caller doesn't wish recorded.

```C++
class TTD::ILiveRecorder::ScopedPauseRecording
{
public:
    ScopedPauseRecording(TTD::ILiveRecorder* pLiveRecorder) noexcept;
    ~ScopedPauseRecording() noexcept;

    bool WasRecording() const noexcept;
};
```

## Constructor

| Name            | Type                                                   | Semantic | Description
|-                |-                                                       |-         |-
| `pLiveRecorder` | [`TTD::ILiveRecorder*`](../interface-ILiveRecorder.md) | In       | Recorder interface used to pause the recording.

The constructor will call the [`TTD::ILiveRecorder::TryPauseRecording`](ILiveRecorder_TryPauseRecording.md) method
via `pLiveRecorder` to pause the recording.

## Destructor

The destructor will call the [`TTD::ILiveRecorder::ResumeRecording`](ILiveRecorder_ResumeRecording.md) method
via the same `pLiveRecorder` that was given to the constructor, if appropriate, to resume the recording.

## `WasRecording` method

This method returns the same value that the [`TTD::ILiveRecorder::TryPauseRecording`](ILiveRecorder_TryPauseRecording.md) method
returned when it was called in the constructor.

This allows the caller to know whether the recording will resume immediately when the `ScopedPauseRecording` object goes out of scope.

## Example of use

```C++
{
    TTD::ILiveRecorder::ScopedPauseRecording const pause{ pRecorder };

    // Recording is definitely paused now.
    // We can do stuff that we don't want in the recording.
}

// Recording has now resumed, if appropriate.
```
