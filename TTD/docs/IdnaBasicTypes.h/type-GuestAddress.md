# GuestAddress Type

Represents a virtual address in the traced (guest) process address space.

## Summary
- Address space: guest process (the program being recorded/replayed)
- Size/representation: 64-bit unsigned integer value
- Comparison: numeric ordering (normal integer semantics)
- Printing: typically as hexadecimal (e.g., 0x7FF612340000)
- Nested constant `GuestAddress::Null` is provided with a value of zero.

## Usage
```cpp
// Create a guest address
GuestAddress entry{ 0x7FF6'12340000ULL };

// Check whether an address falls within a module's range
bool IsAddressInModule(GuestAddress address, Module const& module)
{
    return address >= module.Address &&
           address < module.Address + module.Size;
}

// Check for NULL addresses
if (address != GuestAddress.Null) {
    // Use the address.
}
```

## Notes
- `GuestAddress` is an address in the target program’s address space; it is not a host pointer.
- Use it anywhere an API expects a program (guest) address: module bases, instruction pointers, memory queries, etc.

