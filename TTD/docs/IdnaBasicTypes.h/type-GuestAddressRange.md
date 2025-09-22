# GuestAddressRange Type

Represents a half-open range of guest virtual addresses: [Start, End).

## Members
- Start: GuestAddress — first address in the range (inclusive)
- End: GuestAddress — one past the last address (exclusive)

Common derivation from base+size:
- Start = base
- End = base + size

## Usage
```cpp
// Check if an address is in range
bool IsInRange(GuestAddress addr, GuestAddressRange const& r)
{
    return addr >= r.Start && addr < r.End;
}

// Construct from module
GuestAddressRange text{
    module.Address,
    module.Address + module.Size
};
```

## Notes
- The range is half-open to avoid off-by-one errors and to compose ranges cleanly.
- All addresses are in the guest process address space.

## See also
- [`GuestAddress`](type-GuestAddress.md)
