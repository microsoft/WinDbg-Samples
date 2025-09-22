# ModuleLoadedEvent Structure

Describes an event emitted when a module is loaded into the process during the trace.

## Typical members
- Module: Module — metadata of the loaded module
- Instance: ModuleInstance — the specific instance created
- Position: Position — timeline position when the load was observed

## Usage
```cpp
cursor->SetEventMask(EventMask::ModuleLoad | EventMask::ModuleUnload);
while (cursor->ExecuteForward()) {
    auto evt = cursor->GetLastEvent();
    if (evt.Type == EventType::ModuleLoaded) {
        ModuleLoadedEvent const& mle = evt.As<ModuleLoadedEvent>();
        // Inspect mle.Module and mle.Instance
    }
}
```

## Notes
- Load events correspond to when the loader mapped the module; the module may become executable shortly thereafter.

## See also
- [Module](struct-Module.md)
- [ModuleInstance](struct-ModuleInstance.md)
- [ModuleUnloadedEvent](struct-ModuleUnloadedEvent.md)
