# ModuleUnloadedEvent Structure

Describes an event emitted when a module is unloaded from the process during the trace.

## Typical members
- Module: Module — metadata of the unloaded module
- Instance: ModuleInstance — the specific instance being removed
- Position: Position — timeline position when the unload was observed

## Usage
```cpp
cursor->SetEventMask(EventMask::ModuleLoad | EventMask::ModuleUnload);
while (cursor->ExecuteForward()) {
    auto evt = cursor->GetLastEvent();
    if (evt.Type == EventType::ModuleUnloaded) {
        ModuleUnloadedEvent const& mue = evt.As<ModuleUnloadedEvent>();
        // Inspect mue.Module and mue.Instance
    }
}
```

## Notes
- After unload, addresses within the instance range no longer map to code/data of the module for subsequent positions.

## See also
- [Module](struct-Module.md)
- [ModuleInstance](struct-ModuleInstance.md)
- [ModuleLoadedEvent](struct-ModuleLoadedEvent.md)
