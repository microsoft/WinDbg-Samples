# TTD::ErrorReporting Class

Abstract base class that provides error reporting functionality for TTD APIs.

## Definition

```cpp
namespace TTD
{
    class ErrorReporting
    {
    public:
        ErrorReporting() = default;
        virtual ~ErrorReporting() = default;

        virtual void __fastcall VPrintError(_Printf_format_string_ char const* const pFmt, _In_ va_list argList) = 0;
        void PrintError(_Printf_format_string_ char const* pFmt, ...);
    };
}
```

## Methods

### VPrintError

Pure virtual method that must be implemented to handle error printing with variable arguments.

**Parameters:**
- `pFmt` - Printf-style format string
- `argList` - Variable argument list

**Notes:**
- This method must be implemented by derived classes
- Uses `__fastcall` calling convention

### PrintError

Convenience method that takes variable arguments and forwards them to `VPrintError`.

**Parameters:**
- `pFmt` - Printf-style format string
- `...` - Variable arguments

**Implementation:**
This method uses a `va_list` to collect the variable arguments and calls `VPrintError`.

## Usage

To use this class, derive from `ErrorReporting` and implement the `VPrintError` method to handle error output in the manner appropriate for your application (e.g., logging to file, displaying in UI, etc.).

```cpp
class MyErrorReporter : public TTD::ErrorReporting
{
    void __fastcall VPrintError(char const* const pFmt, va_list argList) override
    {
        // Custom implementation for error reporting
        vprintf(pFmt, argList);
    }
};
```
