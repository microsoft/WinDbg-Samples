# ActiveThreadInfo Structure

Represents a snapshot of active thread state at a given position.

## Typical members
- Thread: ThreadInfo — identity and lifetime
- CurrentPosition: Position — last executed position
- WaitReason: uint32_t — optional wait/scheduling info

## See also
- [ThreadInfo](struct-ThreadInfo.md)
- [Position](struct-Position.md)
