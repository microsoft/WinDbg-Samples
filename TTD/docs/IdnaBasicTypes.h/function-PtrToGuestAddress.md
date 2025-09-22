# PtrToGuestAddress Function

Converts a pointer returned by TTD-mapped buffers into a `GuestAddress` in the guest address space.

```cpp
GuestAddress PtrToGuestAddress(void const* p);
```

## Usage
```cpp
MemoryBuffer mb = cursor->QueryMemoryBuffer(addr, buffer);
void const* p = mb.Data; // host pointer into a mapped view from TTD
GuestAddress ga = PtrToGuestAddress(p);
```

## Important
- Only valid for pointers obtained from TTD APIs that explicitly map guest memory into host buffers.
- Do not pass arbitrary host pointers; the result is undefined.

## See also
- [`GuestAddress`](type-GuestAddress.md)
- [`IReplayEngineView::QueryMemoryBuffer`](../IReplayEngine.h/interface-ICursorView.md) — obtains mapped memory buffers
