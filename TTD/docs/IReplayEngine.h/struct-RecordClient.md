# RecordClient Structure

A "record client" is a component that interacts with the recorder, identified by a user-defined GUID. It can inject custom events, user data, and influence recording (e.g., marking activities, throttling, etc.).

The replay engine gives each record client a unique `RecordClientId`, simpler to use than the GUID for indexing.

Exact definition from `IReplayEngine.h`:

```cpp
// Represents information about a recording client.
// Besides the client's ID, the information is opaque to TTD.
struct RecordClient
{
    RecordClientId  Id;
    GUID            ClientGuid;
    PositionRange   Lifetime;
    ConstBufferView OpenUserData;
    ConstBufferView CloseUserData;

    friend constexpr bool operator==(RecordClient const& a, RecordClient const& b) { return a.Id == b.Id; }
    friend constexpr bool operator!=(RecordClient const& a, RecordClient const& b) { return a.Id != b.Id; }
    friend constexpr bool operator< (RecordClient const& a, RecordClient const& b) { return a.Id <  b.Id; }
    friend constexpr bool operator> (RecordClient const& a, RecordClient const& b) { return a.Id >  b.Id; }
    friend constexpr bool operator<=(RecordClient const& a, RecordClient const& b) { return a.Id <= b.Id; }
    friend constexpr bool operator>=(RecordClient const& a, RecordClient const& b) { return a.Id >= b.Id; }
};

inline size_t RecordClientToString(
                         RecordClient const& client,
    _Out_writes_z_(size) wchar_t*     const  pBuff,
                         size_t       const  size
);
```

## Members
- `RecordClientId Id` – Stable identifier for the recording client.
- `GUID ClientGuid` – User-defined GUID identifying the client component.
- `PositionRange Lifetime` – Range of positions for which the client was active.
- `ConstBufferView OpenUserData` – Opaque user data payload provided when the client opened.
- `ConstBufferView CloseUserData` – Opaque user data payload provided when the client closed.

## Operators
All six comparison operators compare the `Id` field only (see definition above).

## Helper Functions
- `RecordClientToString` – Writes a textual representation including ID, GUID and lifetime.

## See also
- [`CustomEvent`](struct-CustomEvent.md)
- [`RecordClientId`](../IdnaBasicTypes.h/enum-RecordClientId.md)
- [`PositionRange`](struct-PositionRange.md)
- [`ConstBufferView`](../IdnaBasicTypes.h/type-ConstBufferView.md)
