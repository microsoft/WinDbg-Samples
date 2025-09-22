# MemoryBuffer Structure

Represents a buffer of guest memory materialized into a host-accessible view along with metadata.

## Typical members
- Data: void const* — host pointer to mapped data
- Size: size_t — number of bytes mapped
- Address: GuestAddress — guest base address for the buffer
- Ranges: MemoryBufferWithRanges — optional sub-range mapping info

## Usage
```cpp
TBufferView<uint8_t> view{ buffer, capacity };
MemoryBuffer mb = cursor->QueryMemoryBuffer(addr, view);
if (mb.Data && mb.Size > 0) {
    // Interpret mb.Data up to mb.Size bytes
}
```

## Notes
- Use `PtrToGuestAddress` only with pointers returned by TTD mapping APIs.

## See also
- [MemoryRange](struct-MemoryRange.md)
- [MemoryBufferWithRanges](struct-MemoryBufferWithRanges.md)
- [PtrToGuestAddress](../IdnaBasicTypes.h/function-PtrToGuestAddress.md)
