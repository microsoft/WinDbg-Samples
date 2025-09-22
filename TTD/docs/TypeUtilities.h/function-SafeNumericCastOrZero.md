# SafeNumericCastOrZero Template Function

Template function that performs safe numeric casting between types, returning zero if the conversion would lose information.

## Definition

```cpp
template < typename TResult, typename TArgument >
__forceinline
static TResult __fastcall SafeNumericCastOrZero(_In_ TArgument argument);
```

## Template Parameters

- `TResult` - The target type to cast to
- `TArgument` - The source type being cast from

## Parameters

- `argument` - The value to cast

## Return Value

- The cast value if the conversion preserves the original value exactly
- Zero (of type `TResult`) if the conversion would lose information

## Behavior

The function performs a round-trip conversion test:
1. Casts the argument to the target type
2. Casts the result back to the original type
3. Compares the round-trip result with the original value
4. Returns the cast value if they match, otherwise returns zero

## Usage

This function is useful for safely converting between numeric types when you need to ensure no data loss occurs:

```cpp
// Safe conversion that preserves precision
int64_t largeValue = 1000;
int32_t smallValue = SafeNumericCastOrZero<int32_t>(largeValue); // Returns 1000

// Conversion that would lose data - returns zero instead
int64_t tooLarge = 0x100000000LL;  // Larger than int32_t can hold
int32_t result = SafeNumericCastOrZero<int32_t>(tooLarge); // Returns 0

// Float to int conversion
double floatVal = 42.7;
int intVal = SafeNumericCastOrZero<int>(floatVal); // Returns 0 (precision loss)

double exactFloat = 42.0;
int exactInt = SafeNumericCastOrZero<int>(exactFloat); // Returns 42
```

This is particularly useful in APIs where you want to prevent silent data corruption while providing a predictable fallback behavior.
