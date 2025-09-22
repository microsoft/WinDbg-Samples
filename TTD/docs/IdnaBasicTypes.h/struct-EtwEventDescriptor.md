# EtwEventDescriptor Structure

Event descriptor structure for Event Tracing for Windows (ETW) events captured during TTD recording sessions, providing metadata for event analysis and filtering.

## Definition

```cpp
struct EtwEventDescriptor
{
    uint16_t Id;
    uint8_t  Version;
    uint8_t  Channel;
    uint8_t  Level;
    uint8_t  Opcode;
    uint16_t Task;
    uint64_t Keyword;
};
```

## Fields

- `Id` - Event identifier (unique within a provider)
- `Version` - Event version number
- `Channel` - Event channel (Admin, Operational, Analytic, Debug)
- `Level` - Event level (Critical, Error, Warning, Information, Verbose)
- `Opcode` - Event opcode (Start, Stop, Info, etc.)
- `Task` - Task identifier grouping related events
- `Keyword` - 64-bit keyword bitmask for event categorization and filtering

## Event Levels

Standard ETW event levels for severity classification:

```cpp
enum class EtwEventLevel : uint8_t
{
    Critical    = 1,  // Critical errors that cause application failure
    Error       = 2,  // Errors that don't cause immediate failure
    Warning     = 3,  // Warning conditions
    Information = 4,  // Informational events
    Verbose     = 5   // Detailed trace information
};
```

## Event Opcodes

Common ETW event opcodes for operation classification:

```cpp
enum class EtwEventOpcode : uint8_t
{
    Info        = 0,   // General information
    Start       = 1,   // Activity start
    Stop        = 2,   // Activity stop
    DCStart     = 3,   // Data collection start
    DCStop      = 4,   // Data collection stop
    Extension   = 5,   // Extension event
    Reply       = 6,   // Reply event
    Resume      = 7,   // Resume event
    Suspend     = 8,   // Suspend event
    Send        = 9,   // Send event
    Receive     = 240  // Receive event
};
```

## Event Channels

ETW event channels for different purposes:

```cpp
enum class EtwEventChannel : uint8_t
{
    Admin       = 16,  // Administrative events
    Operational = 17,  // Operational events  
    Analytic    = 18,  // Analytic events
    Debug       = 19   // Debug events
};
```

## Important Notes

- ETW events provide rich metadata for application and system analysis
- Keywords are bitmasks allowing multiple categories per event
- Event levels follow standard severity classifications
- Opcodes help track activity start/stop pairs
- Tasks group related events for analysis
- Channel classification helps filter event types
- Version field allows for event schema evolution
- Event descriptors are metadata only - actual event payload is separate
- Proper filtering is essential for managing large event volumes
- Activity tracking requires pairing start/stop events
