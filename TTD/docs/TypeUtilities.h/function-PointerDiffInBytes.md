# PointerDiffInBytes Function

Function that calculates the difference between two pointers in bytes.

## Definition

```cpp
__forceinline ptrdiff_t __fastcall PointerDiffInBytes(_In_opt_ void const* ptr1, _In_opt_ void const* ptr2);
```

## Parameters

- `ptr1` - First pointer (can be null)
- `ptr2` - Second pointer (can be null)

## Return Value

The difference between the two pointers in bytes, calculated as `ptr1 - ptr2`.

## Behavior

The function:
1. Converts both pointers to `intptr_t` values
2. Subtracts `ptr2` from `ptr1`
3. Returns the result as a `ptrdiff_t`

## Usage

This function is useful for calculating memory distances or offsets:

```cpp
char buffer[1000];
char* start = buffer;
char* end = buffer + 500;

ptrdiff_t distance = PointerDiffInBytes(end, start); // Returns 500
ptrdiff_t reverse = PointerDiffInBytes(start, end);  // Returns -500

// Can be used with any pointer types
int* intPtr1 = reinterpret_cast<int*>(0x1000);
int* intPtr2 = reinterpret_cast<int*>(0x1010);
ptrdiff_t diff = PointerDiffInBytes(intPtr1, intPtr2); // Returns -16
```

The function handles null pointers gracefully, though the result with null pointers may not be meaningful in most contexts.

**Note:** This function performs raw pointer arithmetic and should be used carefully. The pointers should ideally point within the same memory region for the result to be meaningful.
