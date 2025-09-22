# IReplayEngineView Interface

The main replay engine interface for querying global trace information and creating cursors. This is a non-owning view interface that provides read-only access to trace data.

## Definition

```cpp
class IReplayEngineView
{
public:
    // System and trace information
    virtual GuestAddress          GetPebAddress()    const noexcept = 0;
    virtual SystemInfo     const& GetSystemInfo()    const noexcept = 0;
    virtual Position       const& GetFirstPosition() const noexcept = 0;
    virtual Position       const& GetLastPosition()  const noexcept = 0;
    virtual PositionRange  const& GetLifetime()      const noexcept = 0;
    virtual RecordingType         GetRecordingType() const noexcept = 0;

    // Thread information
    virtual ThreadInfo const& GetThreadInfo(UniqueThreadId) const noexcept = 0;
    virtual size_t            GetThreadCount()               const noexcept = 0;
    virtual ThreadInfo const* GetThreadList()                const noexcept = 0;

    // Module information  
    virtual size_t        GetModuleCount() const noexcept = 0;
    virtual Module const* GetModuleList()  const noexcept = 0;
    virtual size_t                GetModuleInstanceCount() const noexcept = 0;
    virtual ModuleInstance const* GetModuleInstanceList()  const noexcept = 0;

    // Events
    virtual size_t                     GetModuleLoadedEventCount()   const noexcept = 0;
    virtual ModuleLoadedEvent   const* GetModuleLoadedEventList()    const noexcept = 0;
    virtual size_t                     GetModuleUnloadedEventCount() const noexcept = 0;
    virtual ModuleUnloadedEvent const* GetModuleUnloadedEventList()  const noexcept = 0;

    virtual size_t                       GetThreadCreatedEventCount()    const noexcept = 0;
    virtual ThreadCreatedEvent    const* GetThreadCreatedEventList()     const noexcept = 0;
    virtual size_t                       GetThreadTerminatedEventCount() const noexcept = 0;
    virtual ThreadTerminatedEvent const* GetThreadTerminatedEventList()  const noexcept = 0;

    virtual size_t                GetExceptionEventCount() const noexcept = 0;
    virtual ExceptionEvent const* GetExceptionEventList()  const noexcept = 0;
    virtual ExceptionEvent const* GetExceptionAtOrAfterPosition(Position const&) const noexcept = 0;

    // Custom events and activities
    virtual size_t              GetRecordClientCount() const noexcept = 0;
    virtual RecordClient const* GetRecordClientList()  const noexcept = 0;
    virtual RecordClient const& GetRecordClient(RecordClientId) const noexcept = 0;

    virtual size_t             GetCustomEventCount() const noexcept = 0;
    virtual CustomEvent const* GetCustomEventList()  const noexcept = 0;

    virtual size_t          GetActivityCount() const noexcept = 0;
    virtual Activity const* GetActivityList()  const noexcept = 0;

    // Index and keyframes
    virtual size_t          GetKeyframeCount() const noexcept = 0;
    virtual Position const* GetKeyframeList()  const noexcept = 0;
    virtual size_t        GetIslandCount() const noexcept = 0;
    virtual Island const* GetIslandList()  const noexcept = 0;

    // Cursor creation
    virtual ICursor* NewCursor(GUID const& = __uuidof(ICursorView)) noexcept = 0;

    // Index management
    virtual IndexStatus BuildIndex(
        IndexBuildProgressCallback reportProgress,
        void const*                pCallerContext,
        IndexBuildFlags            flags = IndexBuildFlags::None
    ) noexcept = 0;
    
    virtual IndexStatus    GetIndexStatus()    const noexcept = 0;
    virtual IndexFileStats GetIndexFileStats() const noexcept = 0;

    // Debug and error handling
    virtual void RegisterDebugModeAndLogging(
        DebugModeType   debugMode,
        ErrorReporting* pErrorReporting
    ) noexcept = 0;

protected:
    virtual ~IReplayEngineView() = default;
};
```

## Key Methods

### System Information

#### GetPebAddress()
Returns the Process Environment Block (PEB) address for the traced process.

#### GetSystemInfo()
Returns detailed system information including OS version, CPU architecture, and hardware details.

#### GetLifetime()
Returns the complete timeline range from first to last recorded position.

```cpp
auto lifetime = engine->GetLifetime();
Position start = lifetime.Begin;
Position end = lifetime.End;
```

### Thread Management

#### GetThreadInfo(UniqueThreadId)
Retrieves information about a specific thread by its unique identifier.

#### GetThreadList()
Returns an array of all threads that existed during the trace, with count from `GetThreadCount()`.

```cpp
size_t threadCount = engine->GetThreadCount();
ThreadInfo const* threads = engine->GetThreadList();

for (size_t i = 0; i < threadCount; ++i) {
    printf("Thread %u: OS ID %u\n", 
           threads[i].UniqueId, 
           threads[i].ThreadId);
}
```

### Module Information

#### GetModuleList()
Returns information about all modules (DLLs, executables) referenced in the trace.

#### GetModuleInstanceList()
Returns all module load/unload instances throughout the trace timeline.

### Event Querying

#### GetExceptionEventList()
Returns all exception events that occurred during execution.

#### GetExceptionAtOrAfterPosition(Position)
Finds the first exception at or after the specified position.

```cpp
Position searchPos = GetSomePosition();
ExceptionEvent const* exception = engine->GetExceptionAtOrAfterPosition(searchPos);
if (exception) {
    printf("Exception 0x%X at position %s\n", 
           exception->ExceptionCode, 
           PositionToString(exception->Position).c_str());
}
```

### Cursor Creation

#### NewCursor()
Creates a new cursor for navigating the trace timeline.

```cpp
// Create cursor with default interface
ICursor* cursor = engine->NewCursor();

// Create cursor with specific interface
ICursor* cursor = engine->NewCursor(__uuidof(ICursorView));
```

### Index Management

#### BuildIndex()
Builds the global memory index for faster queries. This is typically done once per trace file.

```cpp
IndexStatus status = engine->BuildIndex(
    [](float progress, void* context) {
        printf("Index build progress: %.1f%%\n", progress * 100);
        return true; // Continue building
    },
    nullptr, // context
    IndexBuildFlags::None
);
```

## Usage Patterns

### Basic Trace Information
```cpp
// Get trace overview
auto lifetime = engine->GetLifetime();
auto recordingType = engine->GetRecordingType();
auto& systemInfo = engine->GetSystemInfo();

printf("Trace spans %llu steps\n", 
       lifetime.End.Steps - lifetime.Begin.Steps);
printf("Recording type: %s\n", 
       GetRecordingTypeName(recordingType));
printf("OS: %S\n", systemInfo.OsVersionString);
```

### Event Analysis
```cpp
// Analyze exceptions
size_t exceptionCount = engine->GetExceptionEventCount();
ExceptionEvent const* exceptions = engine->GetExceptionEventList();

for (size_t i = 0; i < exceptionCount; ++i) {
    auto& ex = exceptions[i];
    printf("Exception 0x%X at step %llu\n", 
           ex.ExceptionCode, 
           ex.Position.Steps);
}
```

### Thread Lifetime Analysis
```cpp
// Find thread creation/termination events
size_t createCount = engine->GetThreadCreatedEventCount();
size_t termCount = engine->GetThreadTerminatedEventCount();

ThreadCreatedEvent const* creates = engine->GetThreadCreatedEventList();
ThreadTerminatedEvent const* terms = engine->GetThreadTerminatedEventList();

// Match creates with terminations
for (size_t i = 0; i < createCount; ++i) {
    auto& create = creates[i];
    printf("Thread %u created at step %llu\n", 
           create.pThreadInfo->ThreadId, 
           create.Position.Steps);
}
```

## See Also

- [`IReplayEngine`](interface-IReplayEngine.md) - Owning engine interface
- [`ICursorView`](interface-ICursorView.md) - Cursor navigation interface  
- [`Position`](struct-Position.md) - Timeline position structure
- [`SystemInfo`](struct-SystemInfo.md) - System information structure
- [`ThreadInfo`](struct-ThreadInfo.md) - Thread information structure
