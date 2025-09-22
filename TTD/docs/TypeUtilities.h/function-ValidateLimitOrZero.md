# ValidateLimitOrZero Template Function

Template function that validates a value against a limit, returning zero if the value exceeds the limit.

## Definition

```cpp
template < typename TArgument, typename TLimit >
__forceinline
TArgument __fastcall ValidateLimitOrZero(_In_ TArgument argument, _In_ TLimit limit);
```

## Template Parameters

- `TArgument` - The type of the value being validated
- `TLimit` - The type of the limit value

## Parameters

- `argument` - The value to validate
- `limit` - The maximum allowed value

## Return Value

- The original `argument` if it is less than or equal to `limit`
- Zero (of type `TArgument`) if `argument` exceeds `limit`

## Behavior

The function performs a simple comparison:
1. If `argument > limit`, returns zero
2. Otherwise, returns the original `argument`

## Usage

This function is useful for bounds checking with a safe fallback:

```cpp
// Valid value within limit
uint32_t value1 = 100;
uint32_t result1 = ValidateLimitOrZero(value1, 200u); // Returns 100

// Value exceeds limit
uint32_t value2 = 300;
uint32_t result2 = ValidateLimitOrZero(value2, 200u); // Returns 0

// Can work with different but comparable types
int value3 = 150;
int result3 = ValidateLimitOrZero(value3, 200u); // Returns 150
```

This is particularly useful for API parameters where you want to reject invalid values in a predictable way, such as buffer sizes or array indices that must not exceed certain limits.
