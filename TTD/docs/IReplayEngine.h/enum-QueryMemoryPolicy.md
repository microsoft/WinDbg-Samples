# QueryMemoryPolicy Enumeration

Specifies policies for reading guest memory via the replay engine.

## Typical values
- Default — Basic best-effort read of committed memory
- RequireContiguous — Only return results if a contiguous read is possible
- AllowPartial — Return partial data if entire range cannot be read

## Usage
```cpp
auto mb = cursor->QueryMemoryBuffer(addr, view, QueryMemoryPolicy::Default);
```

## Notes
- Exact policy names/semantics are determined by the SDK version; consult the header for full details.
