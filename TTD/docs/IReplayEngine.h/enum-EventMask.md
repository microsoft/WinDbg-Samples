# EventMask Enumeration

Bitmask enumeration that controls which types of events will cause cursor execution to stop during `ExecuteForward()` or `ExecuteBackward()` operations.

## Definition

```cpp
enum class EventMask : uint32_t
{
    MemoryWatchpoint   = (1u << EventType::MemoryWatchpoint),
    PositionWatchpoint = (1u << EventType::PositionWatchpoint),
    Exception          = (1u << EventType::Exception),
    Gap                = (1u << EventType::Gap),
    Thread             = (1u << EventType::Thread),

    None = 0,
    All  = MemoryWatchpoint | PositionWatchpoint | Exception | Gap | Thread,

    // Deprecated flags (no longer used)
    ThreadSwitch [[deprecated]] = None,
    Fragment     [[deprecated]] = None,
    Segment      [[deprecated]] = None,
};
```

## Event Types

### MemoryWatchpoint
Stops execution when memory watchpoints are triggered (read, write, execute access to specified memory addresses).

### PositionWatchpoint  
Stops execution when reaching specific positions in the timeline.

### Exception
Stops execution when exceptions occur in the traced program.

### Gap
Stops execution when execution gaps are encountered (context switches, unrecorded regions, etc.).

### Thread
Stops execution when thread-related events occur (creation, termination, etc.).

## Special Values

- `EventMask::None` - No events will cause execution to stop
- `EventMask::All` - All event types will cause execution to stop

## Flag Operations

EventMask supports bitwise operations for combining multiple event types:

```cpp
// Combine multiple event types
EventMask mask = EventMask::MemoryWatchpoint | EventMask::Exception;

// Add an event type
mask |= EventMask::Gap;

// Remove an event type  
mask &= ~EventMask::MemoryWatchpoint;

// Check if specific event is enabled
if (mask & EventMask::Exception) {
    // Exception events are enabled
}
```

## Usage

### Basic Event Filtering
```cpp
// Set up cursor to break on memory accesses and exceptions
cursor->SetEventMask(EventMask::MemoryWatchpoint | EventMask::Exception);

// Execute until one of these events occurs
if (cursor->ExecuteForward()) {
    // Execution stopped due to an event
    EventMask currentMask = cursor->GetEventMask();
    
    if (currentMask & EventMask::MemoryWatchpoint) {
        printf("Stopped due to memory watchpoint\n");
    }
    if (currentMask & EventMask::Exception) {
        printf("Stopped due to exception\n");
    }
}
```

### Stepping Through Exceptions
```cpp
// Only break on exceptions
cursor->SetEventMask(EventMask::Exception);

Position start = cursor->GetPosition();
while (cursor->ExecuteForward()) {
    Position current = cursor->GetPosition();
    
    printf("Exception at position %s\n", 
           PositionToString(current).c_str());
    
    // Continue past this exception
    cursor->SetPosition(current);
    cursor->ExecuteForward(); // Move to next instruction
}
```

### Comprehensive Event Monitoring
```cpp
// Monitor all event types except gaps (for performance)
EventMask mask = EventMask::All & ~EventMask::Gap;
cursor->SetEventMask(mask);

while (cursor->ExecuteForward()) {
    Position pos = cursor->GetPosition();
    EventMask triggered = cursor->GetEventMask();
    
    if (triggered & EventMask::MemoryWatchpoint) {
        ProcessMemoryWatchpoint(cursor, pos);
    }
    if (triggered & EventMask::Exception) {
        ProcessException(cursor, pos);
    }
    if (triggered & EventMask::Thread) {
        ProcessThreadEvent(cursor, pos);
    }
    if (triggered & EventMask::PositionWatchpoint) {
        ProcessPositionWatchpoint(cursor, pos);
    }
}
```

### Conditional Event Enabling
```cpp
// Function to configure events based on analysis needs
void ConfigureCursorForAnalysis(ICursor* cursor, bool includeMemory, bool includeExceptions)
{
    EventMask mask = EventMask::None;
    
    if (includeMemory) {
        mask |= EventMask::MemoryWatchpoint;
    }
    
    if (includeExceptions) {
        mask |= EventMask::Exception;
    }
    
    // Always include position watchpoints for precise navigation
    mask |= EventMask::PositionWatchpoint;
    
    cursor->SetEventMask(mask);
}
```

### Performance Considerations
```cpp
// For high-performance scanning, minimize event overhead
void FastScan(ICursor* cursor, Position start, Position end)
{
    // Only break on critical events
    cursor->SetEventMask(EventMask::Exception);
    
    cursor->SetPosition(start);
    
    while (cursor->GetPosition() < end) {
        if (!cursor->ExecuteForward()) {
            // Reached end or error
            break;
        }
        
        // Process only critical events quickly
        QuickProcessException(cursor);
    }
}
```

### Dynamic Event Mask Adjustment
```cpp
// Adjust event mask based on current analysis phase
class TraceAnalyzer
{
    ICursor* cursor_;
    
public:
    void AnalyzeMemoryAccess()
    {
        // Focus on memory events
        cursor_->SetEventMask(EventMask::MemoryWatchpoint | EventMask::Gap);
        
        while (cursor_->ExecuteForward()) {
            ProcessMemoryEvent();
        }
    }
    
    void AnalyzeControlFlow()
    {
        // Focus on exceptions and thread events
        cursor_->SetEventMask(EventMask::Exception | EventMask::Thread);
        
        while (cursor_->ExecuteForward()) {
            ProcessControlFlowEvent();
        }
    }
};
```

## Integration with Gap Events

When `EventMask::Gap` is enabled, you can further control which types of gaps trigger breaks using `GapKindMask`:

```cpp
// Enable gap events
cursor->SetEventMask(EventMask::Gap);

// But only break on significant gaps
cursor->SetGapKindMask(GapKindMask::Unrecorded | GapKindMask::Large);
```

## See Also

- [`EventType`](enum-EventType.md) - Underlying event type enumeration
- [`GapKindMask`](enum-GapKindMask.md) - Gap event filtering
- [`ICursorView::SetEventMask`](interface-ICursorView.md#seteventmask) - Setting event masks
- [`ICursorView::ExecuteForward`](interface-ICursorView.md#executeforward) - Execution with events
