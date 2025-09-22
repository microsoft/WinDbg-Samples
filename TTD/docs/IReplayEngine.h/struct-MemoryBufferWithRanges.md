# MemoryBufferWithRanges Structure

Describes a memory buffer composed of multiple sub-ranges, each mapping to a part of the guest address space.

## Typical members
- Data: void const* — host pointer to mapped data
- Size: size_t — total size in bytes
- Subranges: array of { Address: GuestAddress, Size: size_t }

## Usage
```cpp
auto mbwr = cursor->QueryMemoryBufferWithRanges(addr, view);
for (auto const& s : mbwr.Subranges) {
    // Each s.Address..s.Address+s.Size is valid data
}
```

## See also
- [MemoryBuffer](struct-MemoryBuffer.md)
- [MemoryRange](struct-MemoryRange.md)
- [GuestAddress](../IdnaBasicTypes.h/type-GuestAddress.md)
