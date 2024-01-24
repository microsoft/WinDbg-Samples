# TTD::ILiveRecorder::Close method

Method of the [`TTD::ILiveRecorder`](interface-ILiveRecorder.md) interface.

Closes the interface, optionally recording a last buffer of data in the trace.

After this method is called, the interface is no longer usable and can be released.
This state can be verified by calling the [`IsOpen`](ILiveRecorder_IsOpen.md) method.
Calling any other methods after this one returns is not fatal,
but TTD's recorder will no longer be reachable so no useful action will be taken.

```C++
void TTD::ILiveRecorder::Close(
    void const* pUserData,
    size_t      userDataSizeInBytes
) noexcept;
```

## Parameters

| Name                  | Type          | Semantic    | Description
|-                      |-              |-            |-
| `pUserData`           | `void const*` | In optional | Pointer to a buffer containing `userDataSizeInBytes` bytes of data to include as "close" user data for the client.
| `userDataSizeInBytes` | `size_t`      | In          | Size in bytes of the `pUserData` buffer. Use `0` if the buffer is not provided.

## Correct use

This method may be freely called from multiple threads simultaneously.
Only one lucky call will record its user data buffer in the trace file
and any subsequent calls will do nothing.

This method has no use restrictions.

## Convenience inlined overloads

Overloads and alternate methods are defined inlined in the header file. They are provided for convenience.

```C++
// Specify the user data buffer as an object.
template < typename UserData >
inline
void TTD::ILiveRecorder::Close(
    UserData const& userData
) noexcept;
```

| Parameter name | Type                | Semantic | Description
|-               |-                    |-         |-
| `userData`     | `typename UserData` | In       | Object provided to be the user data buffer. This object should be of a [standard-layout](https://en.cppreference.com/w/cpp/named_req/StandardLayoutType) type, to ensure it can be recovered from the trace.

## Example of use

```C++
struct CloseUserData
{
    int MeaningOfLife;
};

pRecorder->Close(CloseUserData{ 42 });
```
