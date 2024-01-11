# TTD::TTDMakeLiveRecorder function

Declared in [TTD/TTDLiveRecorder.h](README.md) and implemented in TTDLiveRecorder.dll.

Factory function used to obtain the [`TTD::ILiveRecorder`](interface-ILiveRecorder.md) interface.

```C++
extern "C"
ILiveRecorder* __cdecl TTDMakeLiveRecorder(
    GUID const& clientGuid,
    void const* pUserData,
    size_t      userDataSizeInBytes,
    GUID const& recorderGuid        = __uuidof(ILiveRecorder)
) noexcept;
```

## Parameters

| Name                  | Type          | Semantic    | Description
|-                      |-              |-            |-
| `clientGuid`          | `GUID`        | In          | Specifies the GUID that identifies the recording client. This GUID is defined and provided by the client.
| `pUserData`           | `void const*` | In optional | Pointer to a buffer containing `userDataSizeInBytes` bytes of data to include as "open" user data for the client.
| `userDataSizeInBytes` | `size_t`      | In          | Size in bytes of the `pUserData` buffer. Use `0` if the buffer is not provided.
| `recorderGuid`        | `GUID`        | In          | GUID identifying the interface that is requested. Defaulted, which provides the correct value.

For most uses, `recorderGuid` should be left defaulted. This is not possible if this function is called via a function pointer, in which case `__uuid(TTD::ILiveRecorder)` should be passed explicitly.

The user data buffer is optional. If not provided, `nullptr, 0` should be used.

## Return value

If successful, the return value is the pointer to the interface, ready to be used.

If this function fails, the return value is `nullptr`, no reason provided. This can happen if TTD's recorder is not available in the calling process, or if the recorder is available doesn't support the interface. Either way the caller has no recourse but proceed without the interface or fail.

## Correct use

This function may be freely called from multiple threads simultaneously.

The application calling this function may call this multiple times. Each call represents a different recording client and will return a new recorder access object, so different GUIDs must be provided. The implementation doesn't verify that the GUID provided is unique.

## Convenience inlined overloads

Overloads and alternate functions are defined inlined in the header file. They are provided for convenience.

```C++
// Specify the user data buffer as an object.
template < typename UserData >
inline
ILiveRecorder* TTD::MakeLiveRecorder(
    GUID const&     clientGuid,
    UserData const& userData,
    GUID const&     recorderGuid = __uuidof(ILiveRecorder)
) noexcept;
```

| Parameter name | Type                | Semantic | Description
|-               |-                    |-         |-
| `userData`     | `typename UserData` | In       | Object provided to be the user data buffer. This object should be of a [standard-layout](https://en.cppreference.com/w/cpp/named_req/StandardLayoutType) type, to ensure it can be recovered from the trace.

## Example of use

```C++
// GUID specific to this client.
// {6DA58208-3BF5-4B80-A711-781098BC4445}
static constexpr GUID ClientGuid = { 0x6da58208, 0x3bf5, 0x4b80, { 0xa7, 0x11, 0x78, 0x10, 0x98, 0xbc, 0x44, 0x45 } };

struct OpenUserData
{
    int MeaningOfLife;
};

TTD::ILiveRecorder* pRecorder = TTD::MakeLiveRecorder(ClientGuid, OpenUserData{ 42 });
```
