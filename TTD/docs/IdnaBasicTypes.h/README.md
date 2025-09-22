# TTD/IdnaBasicTypes.h

This header provides core types and definitions for TTD's internal data structures, including sequence management, buffer views, thread identification, and system information structures. It serves as the foundation for TTD's type system and memory management.

It resides in the [Microsoft.TimeTravelDebugging.Apis](../Microsoft.TimeTravelDebugging.Apis.md) NuGet package.

## Error Checking and Diagnostics

- [`ErrorCheckingLevel`](enum-ErrorCheckingLevel.md) - Debug validation levels
- [`GetErrorCheckingLevelInterval`](function-GetErrorCheckingLevelInterval.md) - Get check interval for level
- [`GetErrorCheckingLevelName`](function-GetErrorCheckingLevelName.md) - Get human-readable level name

## Architectural Types

- [`GuestAddress`](type-GuestAddress.md) - Guest process memory addresses
- [`GuestAddressRange`](type-GuestAddressRange.md) - Memory address ranges  
- [`PtrToGuestAddress`](function-PtrToGuestAddress.md) - Pointer to guest address conversion
- [`ProcessorArchitecture`](enum-ProcessorArchitecture.md) - CPU architecture enumeration

## Sequence and Record Management

- [`SequenceId`](enum-SequenceId.md) - Timeline sequence identifier
- [`RecordClientId`](enum-RecordClientId.md) - Recording client identifier

## Buffer and Memory Management

### Template Classes
- [`TBufferView`](template-TBufferView.md) - Generic buffer view template

### Type Aliases
- [`ConstBufferView`](type-ConstBufferView.md) - Read-only buffer view
- [`BufferView`](type-BufferView.md) - Mutable buffer view

## Thread Management

- [`ThreadId`](enum-ThreadId.md) - Thread identifier enumeration

## System Information Structures

### Core System Data
- [`SystemInfo`](struct-SystemInfo.md) - Complete system configuration information
- [`TimingInfo`](struct-TimingInfo.md) - Process and system timing data

### Exception and Event Data
- [`ExceptionRecord64`](struct-ExceptionRecord64.md) - 64-bit exception record structure
- [`EtwEventDescriptor`](struct-EtwEventDescriptor.md) - Event Tracing for Windows descriptor

## Debug Macros

This header uses assertions but enables the assertion mechanism to be defined by the user. The
assertions do nothing if the user has not provided an implementation of its own.

```cpp
#ifndef DBG_ASSERT
#define DBG_ASSERT(cond) do {} while(false)
#endif
#ifndef DBG_ASSERT_MSG
#define DBG_ASSERT_MSG(cond, ...) DBG_ASSERT(cond)
#endif
```

- `DBG_ASSERT(cond)` - Basic assertion macro for condition checking
- `DBG_ASSERT_MSG(cond, ...)` - Assertion with formatted message and arguments

## Configuration Constants

# REVIEW: ErrorCheckingLevel items are record-only types and not used by the live recorder; consider removing or relocating them from this doc.

- `DefaultErrorCheckingLevel` - Default error checking level based on build configuration:
  - `ErrorCheckingLevel::Minimum` in debug builds (`#ifdef DBG`)
  - `ErrorCheckingLevel::Off` in release builds

## Usage Patterns

### Buffer Management

# REVIEW: Decide how many usage samples to include here; exhaustive operator coverage may be unnecessary if IDE IntelliSense is available.

```cpp
// Create buffer views for data processing
uint8_t data[1024];
BufferView buffer{data, sizeof(data)};

// Read-only access
ConstBufferView constBuffer = buffer; // Automatic conversion

// Buffer arithmetic
buffer += 100; // Skip first 100 bytes
```

### System Information Access
```cpp
// Access system configuration
SystemInfo const& sysInfo = GetSystemInfo();
printf("OS Version: %u.%u.%u\n", 
       sysInfo.System.MajorVersion,
       sysInfo.System.MinorVersion, 
       sysInfo.System.BuildNumber);

// Check processor architecture
if (sysInfo.System.ProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) {
    // 64-bit x86 processing
}
```

### Sequence Management
```cpp
// Work with sequence identifiers
SequenceId seq{100};
seq += 50; // Advance sequence

// Calculate sequence differences
SequenceId start{0}, end{1000};
int64_t difference = end - start; // 1000
```

## Thread Safety

- Buffer views are not inherently thread-safe and require external synchronization
- System information structures are typically read-only after initialization
- Sequence and record identifiers are value types and thread-safe for reading

## Other Definitions

- [`SelectRegisters`](enum-SelectRegisters.md) - Register selection flags

## See Also

- [`TypeUtilities.h`](../TypeUtilities.h/README.md) - Type manipulation utilities
- [`IReplayEngine.h`](../IReplayEngine.h/README.md) - Main replay engine interfaces
