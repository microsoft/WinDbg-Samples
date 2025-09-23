# ICursorView Interface

The main cursor interface for navigating through trace timelines and querying execution state at specific positions. Cursors provide context-sensitive access to memory, registers, and thread state.

## Definition

```cpp
class ICursorView
{
public:
    // Memory queries
    virtual MemoryRange QueryMemoryRange(
        GuestAddress address,
        QueryMemoryPolicy policy = QueryMemoryPolicy::Default
    ) const noexcept = 0;

    virtual MemoryBuffer QueryMemoryBuffer(
        GuestAddress address,
        BufferView buffer,
        QueryMemoryPolicy policy = QueryMemoryPolicy::Default
    ) const noexcept = 0;

    virtual MemoryBufferWithRanges QueryMemoryBufferWithRanges(
        GuestAddress address,
        BufferView buffer,
        size_t maxRanges,
        MemoryRange* ranges,
        QueryMemoryPolicy policy = QueryMemoryPolicy::Default
    ) const noexcept = 0;

    // Memory policy control
    virtual void              SetDefaultMemoryPolicy(QueryMemoryPolicy) noexcept = 0;
    virtual QueryMemoryPolicy GetDefaultMemoryPolicy() const noexcept = 0;

    // Context queries for any thread
    virtual ThreadInfo const&       GetThreadInfo(ThreadId = ThreadId::Invalid)          const noexcept = 0;
    virtual GuestAddress            GetTebAddress(ThreadId = ThreadId::Invalid)          const noexcept = 0;
    virtual Position const&         GetPosition(ThreadId = ThreadId::Invalid)            const noexcept = 0;
    virtual Position const&         GetPreviousPosition(ThreadId = ThreadId::Invalid)    const noexcept = 0;
    virtual GuestAddress            GetProgramCounter(ThreadId = ThreadId::Invalid)      const noexcept = 0;
    virtual GuestAddress            GetStackPointer(ThreadId = ThreadId::Invalid)        const noexcept = 0;
    virtual GuestAddress            GetFramePointer(ThreadId = ThreadId::Invalid)        const noexcept = 0;
    virtual uint64_t                GetBasicReturnValue(ThreadId = ThreadId::Invalid)    const noexcept = 0;
    virtual RegisterContext         GetCrossPlatformContext(ThreadId = ThreadId::Invalid) const noexcept = 0;
    virtual ExtendedRegisterContext GetAvxExtendedContext(ThreadId = ThreadId::Invalid)  const noexcept = 0;

    // Module and thread enumeration
    virtual size_t                GetModuleCount() const noexcept = 0;
    virtual ModuleInstance const* GetModuleList()  const noexcept = 0;
    virtual size_t                  GetThreadCount() const noexcept = 0;
    virtual ActiveThreadInfo const* GetThreadList()  const noexcept = 0;

    // Event filtering
    virtual void      SetEventMask(EventMask mask) noexcept = 0;
    virtual EventMask GetEventMask() const noexcept = 0;
    virtual void        SetGapKindMask(GapKindMask mask) noexcept = 0;
    virtual GapKindMask GetGapKindMask() const noexcept = 0;

    // Navigation and execution
    virtual bool SetPosition(Position const&) noexcept = 0;
    virtual bool ExecuteForward()  noexcept = 0;
    virtual bool ExecuteBackward() noexcept = 0;

    // Replay engine access
    template<typename Interface = IReplayEngineView>
    Interface* GetReplayEngine() const noexcept;

protected:
    virtual ~ICursorView() = default;
};
```

## Core Navigation Methods

### SetPosition()
```cpp
bool SetPosition(Position const& position) noexcept;
```
Moves the cursor to the specified timeline position.

**Returns**: `true` if successful, `false` if the position is invalid or inaccessible.

```cpp
Position targetPos{SequenceId{1}, StepCount{1000}};
if (cursor->SetPosition(targetPos)) {
    // Successfully positioned
    auto registers = cursor->GetCrossPlatformContext();
} else {
    // Invalid position
}
```

### ExecuteForward() / ExecuteBackward()
```cpp
bool ExecuteForward() noexcept;
bool ExecuteBackward() noexcept;
```
Executes the trace forward or backward until a breakpoint is hit or the end/beginning is reached.

```cpp
// Set up breakpoints by configuring event mask
cursor->SetEventMask(EventMask::MemoryWatchpoint | EventMask::Exception);

// Execute forward until breakpoint
if (cursor->ExecuteForward()) {
    // Stopped at breakpoint - query why
    EventMask events = cursor->GetEventMask();
    Position stopPos = cursor->GetPosition();
}
```

## Memory Query Methods

### QueryMemoryRange()
```cpp
MemoryRange QueryMemoryRange(GuestAddress address, QueryMemoryPolicy policy = QueryMemoryPolicy::Default) const noexcept;
```
Returns a single contiguous memory range starting at the specified address.

```cpp
GuestAddress addr = cursor->GetStackPointer();
MemoryRange range = cursor->QueryMemoryRange(addr);

if (range.Size > 0) {
    // Access memory data
    uint8_t const* data = static_cast<uint8_t const*>(range.BaseAddress);
    printf("Memory at 0x%llX: %02X %02X %02X...\n",
           addr, data[0], data[1], data[2]);
}
```

### QueryMemoryBuffer()
```cpp
MemoryBuffer QueryMemoryBuffer(GuestAddress address, BufferView buffer, QueryMemoryPolicy policy = QueryMemoryPolicy::Default) const noexcept;
```
Fills a provided buffer with memory data from the specified address.

```cpp
uint8_t buffer[1024];
BufferView bufferView{buffer, sizeof(buffer)};
GuestAddress startAddr = GetTargetAddress();

MemoryBuffer result = cursor->QueryMemoryBuffer(startAddr, bufferView);

printf("Read %zu bytes starting at 0x%llX\n",
       result.ValidSize, startAddr);
```

## Register and Context Queries

### GetCrossPlatformContext()
```cpp
RegisterContext GetCrossPlatformContext(ThreadId threadId = ThreadId::Invalid) const noexcept;
```
Retrieves CPU registers for the specified thread (or current thread if not specified).

```cpp
// Get registers for current thread
RegisterContext regs = cursor->GetCrossPlatformContext();

// Get registers for specific thread
ThreadId targetThread = GetSomeThreadId();
RegisterContext threadRegs = cursor->GetCrossPlatformContext(targetThread);
```

### GetProgramCounter() / GetStackPointer()
```cpp
GuestAddress GetProgramCounter(ThreadId = ThreadId::Invalid) const noexcept;
GuestAddress GetStackPointer(ThreadId = ThreadId::Invalid) const noexcept;
```
Convenience methods for getting key register values.

```cpp
GuestAddress pc = cursor->GetProgramCounter();
GuestAddress sp = cursor->GetStackPointer();

printf("PC: 0x%llX, SP: 0x%llX\n", pc, sp);
```

## Thread and Module Information

### GetThreadList()
```cpp
size_t GetThreadCount() const noexcept;
ActiveThreadInfo const* GetThreadList() const noexcept;
```
Enumerates all threads active at the current position.

```cpp
size_t threadCount = cursor->GetThreadCount();
ActiveThreadInfo const* threads = cursor->GetThreadList();

for (size_t i = 0; i < threadCount; ++i) {
    auto& thread = threads[i];
    printf("Thread %u at position %s\n",
           thread.pThreadInfo->ThreadId,
           PositionToString(thread.Position).c_str());
}
```

### GetModuleList()
```cpp
size_t GetModuleCount() const noexcept;
ModuleInstance const* GetModuleList() const noexcept;
```
Enumerates all modules loaded at the current position.

```cpp
size_t moduleCount = cursor->GetModuleCount();
ModuleInstance const* modules = cursor->GetModuleList();

for (size_t i = 0; i < moduleCount; ++i) {
    auto& mod = modules[i];
    printf("Module: %S at 0x%llX\n",
           mod.pModule->pName,
           mod.pModule->Address);
}
```

## Event Management

### SetEventMask() / GetEventMask()
```cpp
void SetEventMask(EventMask mask) noexcept;
EventMask GetEventMask() const noexcept;
```
Controls which events will cause execution to stop during `ExecuteForward()` or `ExecuteBackward()`.

```cpp
// Set up to break on memory writes and exceptions
EventMask mask = EventMask::MemoryWatchpoint | EventMask::Exception;
cursor->SetEventMask(mask);

// Later, check what events are active
EventMask currentMask = cursor->GetEventMask();
if (currentMask & EventMask::MemoryWatchpoint) {
    // Memory watchpoints are active
}
```

## Memory Policy Control

### SetDefaultMemoryPolicy() / GetDefaultMemoryPolicy()
```cpp
void SetDefaultMemoryPolicy(QueryMemoryPolicy policy) noexcept;
QueryMemoryPolicy GetDefaultMemoryPolicy() const noexcept;
```
Controls how memory queries behave by default.

```cpp
// Use most accurate memory policy
cursor->SetDefaultMemoryPolicy(QueryMemoryPolicy::MostAccurate);

// Query memory - will use MostAccurate policy
MemoryRange range = cursor->QueryMemoryRange(address);
```

## Usage Patterns

### Basic Navigation and State Query
```cpp
// Move to specific position
Position target = GetTargetPosition();
if (cursor->SetPosition(target)) {
    // Query execution state
    GuestAddress pc = cursor->GetProgramCounter();
    RegisterContext regs = cursor->GetCrossPlatformContext();

    // Read memory at PC
    MemoryRange code = cursor->QueryMemoryRange(pc);
    if (code.Size >= 16) {
        // Disassemble instruction at PC
        DisassembleInstruction(code.BaseAddress, pc);
    }
}
```

### Breakpoint-Driven Analysis
```cpp
// Set up memory watchpoint
cursor->SetEventMask(EventMask::MemoryWatchpoint);

// Execute until memory access
while (cursor->ExecuteForward()) {
    // Check what caused the break
    Position currentPos = cursor->GetPosition();

    // Query the memory access that triggered the break
    // (Implementation would need to query event details)

    printf("Memory access at position %s\n",
           PositionToString(currentPos).c_str());
}
```

## See Also

- [`ICursor`](interface-ICursor.md) - Owning cursor interface
- [`Position`](struct-Position.md) - Timeline position structure
- [`RegisterContext`](union-RegisterContext.md) - CPU register context
- [`MemoryRange`](struct-MemoryRange.md) - Memory query results
- [`EventMask`](enum-EventMask.md) - Event filtering options
