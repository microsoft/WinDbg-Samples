# ExceptionRecord64 Structure

64-bit exception record structure containing detailed exception information for TTD analysis, compatible with Windows exception handling mechanisms.

## Definition

```cpp
struct ExceptionRecord64
{
    uint32_t ExceptionCode;
    uint32_t ExceptionFlags;
    uint64_t ExceptionRecord;
    uint64_t ExceptionAddress;
    uint32_t NumberParameters;
    uint32_t Padding0;
    uint64_t ExceptionInformation[15];
};
```

## Fields

- `ExceptionCode` - Exception code identifying the type of exception
- `ExceptionFlags` - Exception flags (continuable, noncontinuable, unwinding, etc.)
- `ExceptionRecord` - Pointer to associated `EXCEPTION_RECORD` (chained exceptions)
- `ExceptionAddress` - Address where the exception occurred
- `NumberParameters` - Number of valid elements in `ExceptionInformation` array
- `Padding0` - Padding for alignment (reserved)
- `ExceptionInformation[15]` - Exception-specific information parameters

## Exception Codes

Common Windows exception codes that may appear in TTD recordings:

### Memory Access Violations
- `0xC0000005` (`EXCEPTION_ACCESS_VIOLATION`) - Access violation
- `0xC000001D` (`EXCEPTION_ILLEGAL_INSTRUCTION`) - Illegal instruction
- `0xC0000096` (`EXCEPTION_PRIVILEGED_INSTRUCTION`) - Privileged instruction

### Arithmetic Exceptions
- `0xC0000094` (`EXCEPTION_INT_DIVIDE_BY_ZERO`) - Integer divide by zero
- `0xC0000095` (`EXCEPTION_INT_OVERFLOW`) - Integer overflow
- `0xC000008E` (`EXCEPTION_FLT_DIVIDE_BY_ZERO`) - Float divide by zero
- `0xC000008F` (`EXCEPTION_FLT_INEXACT_RESULT`) - Float inexact result
- `0xC0000090` (`EXCEPTION_FLT_INVALID_OPERATION`) - Float invalid operation
- `0xC0000091` (`EXCEPTION_FLT_OVERFLOW`) - Float overflow
- `0xC0000092` (`EXCEPTION_FLT_STACK_CHECK`) - Float stack check
- `0xC0000093` (`EXCEPTION_FLT_UNDERFLOW`) - Float underflow

### Control Flow Exceptions
- `0x80000003` (`EXCEPTION_BREAKPOINT`) - Breakpoint
- `0x80000004` (`EXCEPTION_SINGLE_STEP`) - Single step
- `0xC00000FD` (`EXCEPTION_STACK_OVERFLOW`) - Stack overflow

### System Exceptions
- `0xC000013A` (`EXCEPTION_CTRL_C_EXIT`) - Ctrl+C exit
- `0xC0000142` (`EXCEPTION_DLL_INIT_FAILED`) - DLL initialization failed

## Important Notes

- Exception records are 64-bit structures for compatibility with x64 systems
- `ExceptionInformation` array contains up to 15 exception-specific parameters
- The meaning of parameters varies by exception type
- Chained exceptions are linked via the `ExceptionRecord` field
- Exception flags indicate whether execution can continue after handling
- Exception addresses point to the instruction that caused the exception
- Access violations include the access type and target address in parameters
- Breakpoint exceptions are common in debugging scenarios and may be continuable
- Stack overflow exceptions are typically fatal and indicate deep recursion or large local variables

## See Also

- [`TBufferView`](template-TBufferView.md) - Buffer views that may contain exception data
- [`SystemInfo`](struct-SystemInfo.md) - System information that provides context for exception analysis
- [`EtwEventDescriptor`](struct-EtwEventDescriptor.md) - ETW events that may be related to exception reporting
- [`ThreadId`](enum-ThreadId.md) - Thread identification for thread-specific exception handling
