# offsetafter Macro

Macro that returns the byte offset immediately following a specified field in a structure, similar to the standard `offsetof` macro but for the position after the field.

## Definition

```cpp
#define offsetafter(type, field) (reinterpret_cast<size_t>(&static_cast<type*>(nullptr)->field + 1))
```

## Parameters

- `type` - The structure type
- `field` - The field name within the structure

## Return Value

The byte offset immediately following the specified field within the structure.

## Usage

This macro is particularly useful for determining whether it's safe to read a field from a buffer, or for calculating structure sizes up to a certain field:

```cpp
struct MyStruct 
{
    int32_t a;      // offset 0, size 4
    double b;       // offset 8, size 8 (due to alignment)
    char c;         // offset 16, size 1
};

// Get offsets
size_t offset_a = offsetof(MyStruct, a);        // Returns 0
size_t after_a = offsetafter(MyStruct, a);      // Returns 4
size_t offset_b = offsetof(MyStruct, b);        // Returns 8  
size_t after_b = offsetafter(MyStruct, b);      // Returns 16
size_t after_c = offsetafter(MyStruct, c);      // Returns 17

// Check if we have enough data to read field 'b'
bool canReadB = (bufferSize >= offsetafter(MyStruct, b));
```

**Note:** Like `offsetof`, this macro uses pointer arithmetic on a null pointer, which is technically undefined behavior but is widely supported and works correctly in practice on all common platforms.
