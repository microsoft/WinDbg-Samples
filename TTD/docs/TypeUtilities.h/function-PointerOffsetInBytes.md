# PointerOffsetInBytes Template Functions

Template functions that perform pointer arithmetic by adding a byte offset to a pointer.

## Overloads

### Typed Pointer Overload

```cpp
template < typename T, typename V >
__forceinline T* __fastcall PointerOffsetInBytes(_In_ T* ptr, _In_ V offset);
```

**Template Parameters:**
- `T` - The type of the pointer
- `V` - The type of the offset value

**Parameters:**
- `ptr` - The base pointer
- `offset` - The byte offset to add

**Return Value:**
A pointer of type `T*` offset by the specified number of bytes.

### Generic Pointer Overload

```cpp
template < typename T, typename V >
__forceinline T* __fastcall PointerOffsetInBytes(
    _In_ typename std::conditional<std::is_const<T>::value, void const, void>::type* ptr,
    _In_ V offset);
```

**Template Parameters:**
- `T` - The target pointer type to return
- `V` - The type of the offset value

**Parameters:**
- `ptr` - The base pointer (void* or void const* depending on constness of T)
- `offset` - The byte offset to add

**Return Value:**
A pointer of type `T*` offset by the specified number of bytes.

## Behavior

Both overloads:
1. Convert the base pointer to a `uintptr_t`
2. Add the offset (cast to `uintptr_t`)
3. Cast the result back to the target pointer type

The offset can be signed or unsigned - signed values will be sign-extended when cast to `uintptr_t`, providing correct 2's complement arithmetic behavior.

## Usage

### Basic Pointer Arithmetic

```cpp
char buffer[1000];
char* start = buffer;

// Move forward 100 bytes
char* forward = PointerOffsetInBytes(start, 100);

// Move backward 50 bytes (negative offset)
char* backward = PointerOffsetInBytes(forward, -50);
```

### Type Conversion with Offset

```cpp
void* basePtr = GetSomePointer();

// Convert to int* at offset 16
int* intPtr = PointerOffsetInBytes<int*>(basePtr, 16);

// Works with const types too
int const* constIntPtr = PointerOffsetInBytes<int const*>(basePtr, 32);
```

### Struct Member Access

```cpp
struct MyStruct { int a; double b; };
void* structBase = GetStructPointer();

// Access member 'b' at its offset
double* memberB = PointerOffsetInBytes<double*>(structBase, offsetof(MyStruct, b));
```

**Note:** The functions perform raw pointer arithmetic and should be used carefully. Ensure the resulting pointer remains within valid memory bounds.
