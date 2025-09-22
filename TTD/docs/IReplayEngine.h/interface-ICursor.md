# ICursor Interface

Owns a cursor state for navigating the timeline and executing with events.

## Responsibilities
- Holds the mutable navigation state
- Executes forward/backward and reports events
- Provides an `ICursorView` for query APIs

## Typical usage
```cpp
UniqueCursor cursor(engine->NewCursor());
cursor->SetPosition(begin);
cursor->SetEventMask(EventMask::Exception | EventMask::MemoryWatchpoint);
while (cursor->ExecuteForward()) {
    // Inspect events and state via ICursorView
}
```

## See also
- [ICursorView](interface-ICursorView.md)
- [IReplayEngine](interface-IReplayEngine.md)
