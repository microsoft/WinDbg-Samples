# MemoryWatchpointData Structure

Describes a memory access that matched a watchpoint during execution.

## Typical members
- Address: GuestAddress — accessed address
- Size: uint32_t — number of bytes accessed
- Access: DataAccessType — read/write/execute
- Thread: ThreadId — thread that performed the access
- Position: Position — where in the timeline it occurred

## See also
- [DataAccessType](enum-DataAccessType.md)
- [ICursorView](interface-ICursorView.md)
