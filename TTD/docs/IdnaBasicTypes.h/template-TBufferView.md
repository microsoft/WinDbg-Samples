# TBufferView Template

A generic buffer view template providing both const and mutable access to contiguous memory regions. Offers safe, bounds-checked access to buffer data with iterator support.

## Definition

```cpp
template<typename T>
struct TBufferView
{
    T*     Buffer;
    size_t Count;

    // Constructors
    TBufferView() : Buffer(nullptr), Count(0) {}
    TBufferView(T* buffer, size_t count) : Buffer(buffer), Count(count) {}

    // Iterator support
    T* begin() const { return Buffer; }
    T* end() const { return Buffer + Count; }

    // Element access
    T& operator[](size_t index) const { return Buffer[index]; }

    // Properties
    bool IsEmpty() const { return Count == 0 || Buffer == nullptr; }
    size_t SizeInBytes() const { return Count * sizeof(T); }

    // Subview creation
    TBufferView<T> SubView(size_t offset, size_t count) const
    {
        if (offset >= Count) return TBufferView<T>();
        size_t actualCount = std::min(count, Count - offset);
        return TBufferView<T>(Buffer + offset, actualCount);
    }
};
```

## Type Aliases

```cpp
// Common instantiations
using BufferView = TBufferView<uint8_t>;               // Byte buffer view
using ConstBufferView = TBufferView<uint8_t const>;    // Read-only byte buffer view
using WCharBufferView = TBufferView<wchar_t>;          // Wide character buffer view
using ConstWCharBufferView = TBufferView<wchar_t const>; // Read-only wide character buffer view
```

## Usage

### Basic Buffer Operations
```cpp
#include <vector>
#include <iostream>

void DemonstrateBasicUsage()
{
    // Create a buffer
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05};

    // Create buffer view
    BufferView view(data.data(), data.size());

    // Check properties
    printf("Buffer size: %zu bytes, Count: %zu\n", view.SizeInBytes(), view.Count);
    printf("Is empty: %s\n", view.IsEmpty() ? "true" : "false");

    // Element access
    for (size_t i = 0; i < view.Count; ++i) {
        printf("data[%zu] = 0x%02X\n", i, view[i]);
    }

    // Iterator usage
    printf("Data via iterators: ");
    for (auto byte : view) {
        printf("0x%02X ", byte);
    }
    printf("\n");
}
```

### Read-Only Buffer Views
```cpp
void ProcessReadOnlyData(ConstBufferView const& view)
{
    if (view.IsEmpty()) {
        printf("Empty buffer provided\n");
        return;
    }

    // Safe iteration over read-only data
    size_t zeroCount = 0;
    for (auto const& byte : view) {
        if (byte == 0) {
            ++zeroCount;
        }
    }

    printf("Found %zu zero bytes in %zu byte buffer\n", zeroCount, view.Count);
}

void DemonstrateConstViews()
{
    uint8_t const staticData[] = {0x00, 0x01, 0x00, 0x02, 0x00};
    ConstBufferView constView(staticData, sizeof(staticData));

    ProcessReadOnlyData(constView);
}
```

## Important Notes

- Buffer views do not own the underlying memory - ensure the source buffer remains valid
- Always check `IsEmpty()` before accessing buffer data
- Use const buffer views when read-only access is sufficient
- Subviews share the same underlying memory as the parent view
- Iterator support allows use with standard library algorithms
- Thread safety depends on the underlying buffer's thread safety
- Consider memory alignment when casting between different types
