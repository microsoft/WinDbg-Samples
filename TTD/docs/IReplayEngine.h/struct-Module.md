# Module Structure

Represents an individual module (executable or DLL) used during the trace file's timeline. Modules are identified by their metadata and base address.

## Definition

```cpp
struct Module
{
    wchar_t const* pName;       // Module name (informational only)
    size_t         NameLength;  // Length of name string
    GuestAddress   Address;     // Base address where module is loaded
    uint64_t       Size;        // Size of module in bytes
    uint32_t       Checksum;    // Module checksum
    uint32_t       Timestamp;   // Module timestamp

    // Comparison operators compare Address, Timestamp, Checksum, Size
    friend constexpr bool operator==(Module const& a, Module const& b);
    friend constexpr bool operator!=(Module const& a, Module const& b);
    friend constexpr bool operator< (Module const& a, Module const& b);
    friend constexpr bool operator> (Module const& a, Module const& b);
    friend constexpr bool operator<=(Module const& a, Module const& b);
    friend constexpr bool operator>=(Module const& a, Module const& b);
};
```

## Members

### pName
- **Type**: `wchar_t const*`
- **Description**: Module name (e.g., "kernel32.dll", "myapp.exe")
- **Note**: For informational purposes only - not used in comparisons

### NameLength  
- **Type**: `size_t`
- **Description**: Length of the name string in characters

### Address
- **Type**: `GuestAddress`
- **Description**: Base address where the module is loaded in the traced process

### Size
- **Type**: `uint64_t` 
- **Description**: Size of the module in bytes

### Checksum
- **Type**: `uint32_t`
- **Description**: Module checksum for identification

### Timestamp
- **Type**: `uint32_t`
- **Description**: Module timestamp for identification

## Comparison Behavior

Modules are compared using a tuple of `(Address, Timestamp, Checksum, Size)`. The name is **not** included in comparisons, as it's considered informational only.

```cpp
Module mod1 = {L"app.exe", 7, GuestAddress{0x400000}, 0x10000, 0x12345, 0x67890};
Module mod2 = {L"different.exe", 13, GuestAddress{0x400000}, 0x10000, 0x12345, 0x67890};

// These are considered equal despite different names
assert(mod1 == mod2); // True - names not compared
```

## Usage

### Module Enumeration
```cpp
// Get all modules referenced in the trace
size_t moduleCount = engine->GetModuleCount();
Module const* modules = engine->GetModuleList();

for (size_t i = 0; i < moduleCount; ++i) {
    auto& module = modules[i];
    
    wprintf(L"Module: %.*s\n", 
            static_cast<int>(module.NameLength), 
            module.pName);
    printf("  Address: 0x%llX\n", module.Address);
    printf("  Size: %llu bytes\n", module.Size);
    printf("  Checksum: 0x%08X\n", module.Checksum);
    printf("  Timestamp: 0x%08X\n", module.Timestamp);
}
```

### Module Identification
```cpp
// Find a specific module by name
Module const* FindModuleByName(Module const* modules, size_t count, wchar_t const* name)
{
    for (size_t i = 0; i < count; ++i) {
        if (wcsncmp(modules[i].pName, name, modules[i].NameLength) == 0 &&
            wcslen(name) == modules[i].NameLength) {
            return &modules[i];
        }
    }
    return nullptr;
}

// Usage
Module const* kernel32 = FindModuleByName(modules, moduleCount, L"kernel32.dll");
if (kernel32) {
    printf("kernel32.dll loaded at 0x%llX\n", kernel32->Address);
}
```

### Address Range Checking
```cpp
// Check if an address belongs to a module
bool IsAddressInModule(GuestAddress address, Module const& module)
{
    return address >= module.Address && 
           address < module.Address + module.Size;
}

// Find module containing an address
Module const* FindModuleContainingAddress(GuestAddress address, Module const* modules, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        if (IsAddressInModule(address, modules[i])) {
            return &modules[i];
        }
    }
    return nullptr;
}
```

### Module Sorting and Searching
```cpp
#include <algorithm>
#include <vector>

// Create sortable list of modules
std::vector<Module> moduleVec(modules, modules + moduleCount);

// Sort by address (primary sort key)
std::sort(moduleVec.begin(), moduleVec.end());

// Binary search for module at specific address
Module searchKey = {};
searchKey.Address = targetAddress;

auto it = std::lower_bound(moduleVec.begin(), moduleVec.end(), searchKey);
if (it != moduleVec.end() && it->Address == targetAddress) {
    printf("Found module at target address\n");
}
```

## Helper Functions

The header provides a string formatting helper:

### ModuleToString()
```cpp
size_t ModuleToString(
    Module const& module,
    wchar_t* buffer,
    size_t bufferSize
);
```

Formats module information into a string.

```cpp
wchar_t buffer[512];
size_t written = ModuleToString(module, buffer, sizeof(buffer)/sizeof(wchar_t));
if (written > 0) {
    wprintf(L"%s\n", buffer);
}
```

## Important Notes

- The same module binary loaded at different addresses creates distinct `Module` objects
- Module names are provided for convenience but should not be relied upon for identification
- Use checksum and timestamp for reliable module identification
- Address ranges help determine which module contains a specific memory address
- Module information is available globally through `IReplayEngineView::GetModuleList()`

## See Also

- [`ModuleInstance`](struct-ModuleInstance.md) - Module load/unload lifetime information
- [`ModuleLoadedEvent`](struct-ModuleLoadedEvent.md) - Module loading events
- [`ModuleUnloadedEvent`](struct-ModuleUnloadedEvent.md) - Module unloading events
- [`GuestAddress`](../IdnaBasicTypes.h/type-GuestAddress.md) - Guest process addresses
