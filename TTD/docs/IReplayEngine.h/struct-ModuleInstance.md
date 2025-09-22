# ModuleInstance Structure

Represents a specific load lifetime of a module within the trace timeline. A module binary may have multiple instances if it is loaded/unloaded multiple times.

## Summary
- Identifies a module load with its base address and size
- Captures load/unload timeline positions
- Associates back to the immutable Module metadata

## Typical members
- Module: Module — immutable module metadata
- Address: GuestAddress — base address of this instance
- Size: uint64_t — size in bytes
- LoadPosition: Position — timeline position when the instance became active
- UnloadPosition: Position — timeline position when the instance became inactive (or Position::Max if still active at end)

## Usage
```cpp
// Enumerate instances for a module
size_t count = engine->GetModuleInstanceCount();
ModuleInstance const* instances = engine->GetModuleInstanceList();

for (size_t i = 0; i < count; ++i) {
    auto const& inst = instances[i];
    if (inst.Module == targetModule) {
        // Check whether a position is within instance lifetime
        bool active = (pos >= inst.LoadPosition) && (pos < inst.UnloadPosition);
        // Check whether an address lies in this instance's range
        bool inRange = (addr >= inst.Address) && (addr < inst.Address + inst.Size);
    }
}
```

## Notes
- Multiple ModuleInstance entries can reference the same Module metadata if it was reloaded at different times.
- Use lifetime and range checks together when attributing addresses to modules at a given timeline position.

## See also
- [Module](struct-Module.md)
- [ModuleLoadedEvent](struct-ModuleLoadedEvent.md)
- [ModuleUnloadedEvent](struct-ModuleUnloadedEvent.md)
