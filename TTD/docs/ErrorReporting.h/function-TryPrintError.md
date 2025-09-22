# TTD::TryPrintError Functions

Utility functions that safely attempt to print error messages through an `ErrorReporting` object, handling null pointer cases gracefully.

## Overloads

### Printf-style Overload

```cpp
inline void TryPrintError(
    _Inout_opt_            ErrorReporting * const pErrorReporting,
    _Printf_format_string_ char const     * const pFmt, ...);
```

**Parameters:**
- `pErrorReporting` - Pointer to ErrorReporting object (can be null)
- `pFmt` - Printf-style format string
- `...` - Variable arguments for formatting

**Behavior:**
- If `pErrorReporting` is not null, formats the arguments and calls `VPrintError`
- If `pErrorReporting` is null, the function does nothing
- Uses `va_list` internally to handle variable arguments

## Usage

These functions provide a safe way to report errors when you might not have a valid `ErrorReporting` object:

```cpp
TTD::ErrorReporting* reporter = GetErrorReporter(); // might return nullptr

// Safe to call even if reporter is null
TTD::TryPrintError(reporter, "Operation failed with code: %d", errorCode);
TTD::TryPrintError(reporter, std::string("Simple error message"));
```

The functions eliminate the need for null checks in calling code, making error reporting more convenient and less error-prone.
