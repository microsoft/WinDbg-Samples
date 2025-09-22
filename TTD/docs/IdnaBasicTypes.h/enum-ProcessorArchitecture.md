# ProcessorArchitecture Enumeration

Enumeration used to identify a specific CPU architecture.

```cpp
enum class ProcessorArchitecture : uint8_t
{
    Invalid = 0,
    x86     = 1,
    x64     = 2,
    ARM32   = 3,
    Arm64   = 4,
};

constexpr char const* GetProcessorArchitectureName(ProcessorArchitecture architecture);

constexpr ProcessorArchitecture c_nativeProcessorArchitecture = ...;
```

## Enumerators
- `Invalid` – Unspecified / not set.
- `x86` – 32-bit Intel/AMD.
- `x64` – 64-bit AMD64 / x86-64.
- `ARM32` – 32-bit ARM.
- `Arm64` – 64-bit ARM (doesn't include ARM64EC).

## Related Helpers
- `GetProcessorArchitectureName` – Returns a stable string identifier (e.g. `"x64"`).
- `c_nativeProcessorArchitecture` – Compile-time constant for the build target architecture.

## See also
- [`SystemInfo`](struct-SystemInfo.md)
