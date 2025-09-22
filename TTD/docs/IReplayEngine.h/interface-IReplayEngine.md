# IReplayEngine Interface

Owning interface for the TTD Replay Engine that extends `IReplayEngineView` with initialization and lifetime management capabilities. This interface is responsible for loading trace files and manages the engine's lifecycle.

## Definition

```cpp
class IReplayEngine : public IReplayEngineView
{
public:
    friend Deleter<IReplayEngine>;

    // Lifecycle management
    virtual bool Initialize(_In_z_ wchar_t const* pTraceFileName) noexcept = 0;

private:
    // Called by Deleter<IReplayEngine> for proper cleanup
    virtual void Destroy() noexcept = 0;
};
```

## Key Methods

### Initialize()
```cpp
virtual bool Initialize(_In_z_ wchar_t const* pTraceFileName) noexcept = 0;
```

Initializes the replay engine with a trace file. This method may only be called once per engine instance.

**Parameters:**
- `pTraceFileName` - Path to the TTD trace file (`.ttd` or `.run` file)

**Returns:**
- `true` if initialization succeeded
- `false` if initialization failed (invalid file, corrupt trace, etc.)

**Usage:**
```cpp
auto [engine, result] = TTD::Replay::MakeReplayEngine();
if (result == S_OK) {
    if (engine->Initialize(L"C:\\traces\\mytrace.ttd")) {
        // Engine is ready for use
        auto cursor = TTD::Replay::UniqueCursor(engine->NewCursor());
        // ... use cursor for analysis
    } else {
        printf("Failed to initialize engine with trace file\n");
    }
}
```

### Destroy() (Private)
```cpp
virtual void Destroy() noexcept = 0;
```

Private method called by `Deleter<IReplayEngine>` to properly clean up engine resources. Not intended for direct use - use smart pointers instead.

## Inherited Interface

`IReplayEngine` inherits all methods from [`IReplayEngineView`](interface-IReplayEngineView.md), providing:

- System and trace information queries
- Thread and module enumeration
- Event and exception access  
- Cursor creation
- Index building and management
- Debug logging configuration

## Lifecycle Management

### Creation and Initialization
```cpp
// Create engine (uninitialized)
auto [engine, result] = TTD::Replay::MakeReplayEngine();
if (result != S_OK) {
    // Handle creation failure
    return;
}

// Initialize with trace file (required before use)
if (!engine->Initialize(L"trace.ttd")) {
    // Handle initialization failure
    return;
}

// Engine is now ready for use
auto systemInfo = engine->GetSystemInfo();
auto lifetime = engine->GetLifetime();
```

### Single Initialization Rule
```cpp
// CORRECT: Initialize once
engine->Initialize(L"trace1.ttd"); // Returns true

// INCORRECT: Cannot initialize again
engine->Initialize(L"trace2.ttd"); // Behavior undefined
```

### Automatic Cleanup
```cpp
void AnalyzeMultipleTraces(std::vector<std::wstring> const& traceFiles)
{
    for (auto const& filename : traceFiles) {
        // Create new engine for each trace
        auto [engine, result] = TTD::Replay::MakeReplayEngine();
        if (result == S_OK && engine->Initialize(filename.c_str())) {
            
            // Analyze this trace
            AnalyseSingleTrace(engine.get());
            
            // engine automatically destroyed at end of loop iteration
        }
    }
}
```

## Usage Patterns

### Basic Trace Loading
```cpp
TTD::Replay::UniqueReplayEngine LoadTrace(wchar_t const* filename)
{
    auto [engine, result] = TTD::Replay::MakeReplayEngine();
    if (result == S_OK) {
        if (engine->Initialize(filename)) {
            return std::move(engine);
        } else {
            wprintf(L"Failed to load trace: %s\n", filename);
        }
    } else {
        printf("Failed to create replay engine: 0x%X\n", result);
    }
    
    return nullptr;
}
```

### Error Handling Strategies
```cpp
enum class TraceLoadResult
{
    Success,
    EngineCreationFailed,
    InitializationFailed,
    FileNotFound
};

std::pair<TTD::Replay::UniqueReplayEngine, TraceLoadResult> 
LoadTraceWithDetailedErrors(wchar_t const* filename)
{
    // Check file existence first
    if (_waccess(filename, 0) != 0) {
        return {nullptr, TraceLoadResult::FileNotFound};
    }
    
    auto [engine, result] = TTD::Replay::MakeReplayEngine();
    if (result != S_OK) {
        return {nullptr, TraceLoadResult::EngineCreationFailed};
    }
    
    if (!engine->Initialize(filename)) {
        return {nullptr, TraceLoadResult::InitializationFailed};
    }
    
    return {std::move(engine), TraceLoadResult::Success};
}
```

## Important Notes

- **Single Initialization**: Each engine instance can only be initialized once
- **File Format**: Supports `.ttd`, `.run`, and index files  
- **Thread Safety**: Engine operations are not inherently thread-safe
- **Resource Management**: Always use smart pointers (`UniqueReplayEngine`) for automatic cleanup
- **Initialization Order**: Must initialize before calling other engine methods

## See Also

- [`IReplayEngineView`](interface-IReplayEngineView.md) - Base interface with query methods
- [`TTD::Replay::MakeReplayEngine`](../IReplayEngineStl.h/function-MakeReplayEngine.md) - Factory function
- [`Deleter<IReplayEngine>`](template-Deleter.md) - Proper cleanup mechanism
- [`ICursor`](interface-ICursor.md) - Timeline navigation interface
