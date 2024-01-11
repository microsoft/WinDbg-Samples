# TTD::ILiveRecorder::StartRecordingCurrentThread method

Method of the [`TTD::ILiveRecorder`](interface-ILiveRecorder.md) interface.

Starts recording a new island on the calling thread.
If the thread was already being recorded, the recording will momentarily stop before starting the new island.

```C++
void TTD::ILiveRecorder::StartRecordingCurrentThread(
    ActivityId       activity,
    InstructionCount maxInstructionsToRecord,
    void const*      pUserData,
    size_t           userDataSizeInBytes
) noexcept;
```

## Parameters

| Name                      | Type                                                                    | Semantic    | Description
|-                          |-                                                                        |-            |-
| `activity`                | [`TTD::ActivityId`](../TTDCommonTypes.h/type-ActivityId.md)             | In          | Activity ID to assign the new island to.
| `maxInstructionsToRecord` | [`TTD::InstructionCount`](../TTDCommonTypes.h/type-InstructionCount.md) | In          | New instruction count to use as a throttle.
| `pUserData`               | `void const*`                                                           | In optional | Pointer to a buffer containing `userDataSizeInBytes` bytes of data to include as "island" user data for the client.
| `userDataSizeInBytes`     | `size_t`                                                                | In          | Size in bytes of the `pUserData` buffer. Use `0` if the buffer is not provided.

The caller controls and decides the meaning of the value passed in `activity`.

If `maxInstructionsToRecord` is set to `TTD::InstructionCount::Invalid` then there will be no throttle
and recording will proceed until it's stopped by other means.

## Correct use

This method only affects the recording of the calling thread and may be freely called from multiple threads simultaneously.

This method has no use restrictions.

## Convenience inlined overloads

Overloads and alternate methods are defined inlined in the header file. They are provided for convenience.

```C++
// Specify the user data buffer as an object.
template < typename UserData >
inline
void TTD::ILiveRecorder::StartRecordingCurrentThread(
    TTD::ActivityId       activity,
    TTD::InstructionCount maxInstructionsToRecord,
    UserData const&       userData
) noexcept;
```

```C++
// No user data for the island, because this should be fairly common.
inline
void TTD::ILiveRecorder::StartRecordingCurrentThread(
    TTD::ActivityId       activity,
    TTD::InstructionCount maxInstructionsToRecord
) noexcept;
```

### Convenience parameters

| Name       | Type                | Semantic | Description
|-           |-                    |-         |-
| `userData` | `typename UserData` | In       | Object provided to be the user data buffer. This object should be of a [standard-layout](https://en.cppreference.com/w/cpp/named_req/StandardLayoutType) type, to ensure it can be recovered from the trace.

## Example of use

```C++
constexpr TTD::ActivityId MyActivity{ 1234 };

pRecorder->StartRecordingCurrentThread(MyActivity, TTD::InstructionCount::Invalid);

// Any code here will be recorded as part of MyActivity.

pRecorder->StopRecordingCurrentThread();
```
