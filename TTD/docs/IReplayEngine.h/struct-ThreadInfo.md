# ThreadInfo Structure

Describes a thread known to the trace.

## Typical members
- ThreadId: ThreadId — numeric identifier
- UniqueId: UniqueThreadId — stable identifier across timeline
- StartPosition: Position — when the thread started
- EndPosition: Position — when the thread ended (or Position::Max)

## See also
- [ActiveThreadInfo](struct-ActiveThreadInfo.md)
- [ThreadCreatedEvent](struct-ThreadCreatedEvent.md)
- [ThreadTerminatedEvent](struct-ThreadTerminatedEvent.md)
- [UniqueThreadId](type-UniqueThreadId.md)
- [Position](struct-Position.md)
