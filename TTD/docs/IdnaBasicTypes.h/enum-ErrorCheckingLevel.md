# ErrorCheckingLevel Enumeration

Defines the level of internal error checking and validation that TTD APIs perform during operation. Higher levels provide more thorough validation at the cost of performance.

## Definition

```cpp
enum class ErrorCheckingLevel : uint8_t
{
    Off      = 0,
    Minimum  = 1,
    Debug    = 2,
    Paranoid = 3
};
```

## Configuration

inline uint64_t GetErrorCheckingLevelInterval(ErrorCheckingLevel level);
```
Returns the interval at which error checks are performed:
- `Minimum`: Every 1,000 operations
- `Debug`: Every 10 operations  
- `Paranoid`: Every operation (1)
- `Off`: Very large interval (effectively disabled)

### GetErrorCheckingLevelName()
```cpp
inline char const* GetErrorCheckingLevelName(ErrorCheckingLevel level);
```
Returns human-readable name for the error checking level.

## Important Notes

- Error checking levels affect internal TTD validation only
- Higher levels can significantly impact performance
- Level changes may require TTD component restart to take effect
- Some critical safety checks are always enabled regardless of level
- The `DBG_ASSERT` macros used throughout TTD respect this level setting

## See Also

- [`GetErrorCheckingLevelInterval`](function-GetErrorCheckingLevelInterval.md) - Check interval function
- [`GetErrorCheckingLevelName`](function-GetErrorCheckingLevelName.md) - Name lookup function
- Debug macro configuration in TTD headers
