# MemoryRange Structure

Describes a contiguous region of guest memory, its size, and basic attributes.

## Typical members
- Address: GuestAddress — base address of the region
- Size: uint64_t — number of bytes
- Protection: uint32_t — page protection/mapping flags (if exposed)

## Usage
```cpp
MemoryRange r = cursor->QueryMemoryRange(addr);
if (addr >= r.Address && addr < r.Address + r.Size) {
    // Inside region
}
```

## See also
- [MemoryBuffer](struct-MemoryBuffer.md)
- [QueryMemoryPolicy](enum-QueryMemoryPolicy.md)
- [GuestAddress](../IdnaBasicTypes.h/type-GuestAddress.md)
