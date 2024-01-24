# TTD::ILiveRecorder::AddCustomEvent method

Method of the [`TTD::ILiveRecorder`](interface-ILiveRecorder.md) interface.

Places an event marker with associated data in the trace file.

```C++
void TTD::ILiveRecorder::AddCustomEvent(
    TTD::CustomEventType  type,
    TTD::CustomEventFlags flags,
    void const*           pUserData,
    size_t                userDataSizeInBytes
) noexcept;
```

## Parameters

| Name                  | Type                                                                    | Semantic    | Description
|-                      |-                                                                        |-            |-
| `type`                | [`TTD::CustomEventType`](../TTDCommonTypes.h/enum-CustomEventType.md)   | In          | 
| `flags`               | [`TTD::CustomEventFlags`](../TTDCommonTypes.h/type-CustomEventFlags.md) | In          | 
| `pUserData`           | `void const*`                                                           | In optional | Pointer to a buffer containing `userDataSizeInBytes` bytes of data to include as "custom event" user data for the client.
| `userDataSizeInBytes` | `size_t`                                                                | In          | Size in bytes of the `pUserData` buffer. Use `0` if the buffer is not provided.

## Correct use

If the event type is `ThreadLocal`, this method only affects the recording of the calling thread and may be freely called from multiple threads simultaneously. However, note that, if the calling thread is not recorded, a `ThreadLocal` event will be recorded as `Global`.

If the event type is `Global`, this method uses synchronization such that it may be freely called from multiple threads simultaneously.
If there is high contention, the method may take an arbitrarily amount of time to complete.

This method has no use restrictions.

## Convenience inlined overloads

Overloads and alternate methods are defined inlined in the header file. They are provided for convenience.

```C++
// Specify the user data buffer as an object.
template < typename UserData >
inline
void AddCustomEvent(
    TTD::CustomEventType  type,
    TTD::CustomEventFlags flags,
    UserData const&       userData
) noexcept;
```

### Convenience parameters

| Name       | Type                | Semantic | Description
|-           |-                    |-         |-
| `userData` | `typename UserData` | In       | Object provided to be the user data buffer. This object should be of a [standard-layout](https://en.cppreference.com/w/cpp/named_req/StandardLayoutType) type, to ensure it can be recovered from the trace.

## Example of use

```C++
enum class EventId : uint32_t { Checkpoint, Error };
struct CustomEventData
{
    EventId  Id;
    uint32_t Payload;
};

// Something didn't work right, embed an error code in the trace file.
pRecorder->AddCustomEvent(
    TTD::CustomEventType::ThreadLocal,
    TTD::CustomEventFlags::None,
    CustomEventData{ EventId::Error, errorCode }
);
```
