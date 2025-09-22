# RecordClientId Enumeration

Represents unique identifiers for recording clients in the TTD system. Each client that initiates or manages recording activities is assigned a distinct identifier.

## Definition

```cpp
enum class RecordClientId : uint32_t
{
    // Client IDs are ordered, so provide explicit valid min/max.
    Min     = 1,
    Max     = UINT32_MAX - 1,

    Invalid = 0,
    System  = UINT32_MAX,
};
```

## Values

- `RecordClientId::Invalid` (0) - Invalid or uninitialized client ID
- `RecordClientId::Min` (1) - Minimum valid user client identifier
- `RecordClientId::Max` (UINT32_MAX - 1) - Maximum valid user client identifier
- `RecordClientId::System` (UINT32_MAX) - Reserved for system-level recording operations

## Client Types

### User Clients (Min to Max)
Regular recording clients initiated by user applications, debuggers, or analysis tools.

### System Client
Reserved identifier for system-level recording operations, kernel-mode recording, or internal TTD infrastructure.

## Usage

# REVIEW: The detailed client registry examples below may be more than needed here; consider replacing with a shorter validation snippet or moving to an appendix.

### Client Registration and Validation
```cpp
#include <unordered_set>

class ClientManager
{
    std::unordered_set<uint32_t> activeClients_;
    RecordClientId nextClientId_{RecordClientId::Min};
    
public:
    RecordClientId AllocateClientId()
    {
        // Find next available client ID
        while (nextClientId_ != RecordClientId::System && 
               activeClients_.count(static_cast<uint32_t>(nextClientId_)) > 0)
        {
            nextClientId_ = static_cast<RecordClientId>(
                static_cast<uint32_t>(nextClientId_) + 1);
        }
        
        if (nextClientId_ == RecordClientId::System) {
            return RecordClientId::Invalid; // No more IDs available
        }
        
        RecordClientId clientId = nextClientId_;
        activeClients_.insert(static_cast<uint32_t>(clientId));
        nextClientId_ = static_cast<RecordClientId>(
            static_cast<uint32_t>(nextClientId_) + 1);
        
        return clientId;
    }
    
    bool IsValidClient(RecordClientId clientId) const
    {
        return clientId != RecordClientId::Invalid &&
               (activeClients_.count(static_cast<uint32_t>(clientId)) > 0 ||
                clientId == RecordClientId::System);
    }
    
    void ReleaseClientId(RecordClientId clientId)
    {
        if (clientId != RecordClientId::Invalid && clientId != RecordClientId::System) {
            activeClients_.erase(static_cast<uint32_t>(clientId));
        }
    }
    
    size_t GetActiveClientCount() const { return activeClients_.size(); }
};
```

### Client Information Tracking
```cpp
#include <string>
#include <unordered_map>
#include <chrono>

struct ClientInfo
{
    std::string name;
    std::string description;
    std::chrono::steady_clock::time_point startTime;
    bool isSystemClient;
    uint32_t recordingCount;
};

class ClientRegistry
{
    std::unordered_map<uint32_t, ClientInfo> clientInfo_;
    
public:
    bool RegisterClient(RecordClientId clientId, ClientInfo const& info)
    {
        if (clientId == RecordClientId::Invalid) {
            return false;
        }
        
        ClientInfo clientData = info;
        clientData.isSystemClient = (clientId == RecordClientId::System);
        clientData.startTime = std::chrono::steady_clock::now();
        clientData.recordingCount = 0;
        
        clientInfo_[static_cast<uint32_t>(clientId)] = std::move(clientData);
        return true;
    }
    
    ClientInfo const* GetClientInfo(RecordClientId clientId) const
    {
        auto it = clientInfo_.find(static_cast<uint32_t>(clientId));
        return (it != clientInfo_.end()) ? &it->second : nullptr;
    }
    
    void IncrementRecordingCount(RecordClientId clientId)
    {
        auto it = clientInfo_.find(static_cast<uint32_t>(clientId));
        if (it != clientInfo_.end()) {
            ++it->second.recordingCount;
        }
    }
    
    std::vector<RecordClientId> GetActiveClients() const
    {
        std::vector<RecordClientId> clients;
        clients.reserve(clientInfo_.size());
        
        for (auto const& pair : clientInfo_) {
            clients.push_back(static_cast<RecordClientId>(pair.first));
        }
        
        return clients;
    }
    
    std::vector<RecordClientId> GetSystemClients() const
    {
        std::vector<RecordClientId> systemClients;
        
        for (auto const& pair : clientInfo_) {
            if (pair.second.isSystemClient) {
                systemClients.push_back(static_cast<RecordClientId>(pair.first));
            }
        }
        
        return systemClients;
    }
};
```

### Client-Based Recording Operations
```cpp
// Example recording session with client tracking
class RecordingSession
{
    RecordClientId clientId_;
    std::string sessionName_;
    bool isActive_;
    
public:
    RecordingSession(RecordClientId clientId, std::string const& sessionName)
        : clientId_(clientId), sessionName_(sessionName), isActive_(false)
    {
        if (clientId == RecordClientId::Invalid) {
            throw std::invalid_argument("Invalid client ID for recording session");
        }
    }
    
    RecordClientId GetClientId() const { return clientId_; }
    
    bool StartRecording()
    {
        if (clientId_ == RecordClientId::Invalid || isActive_) {
            return false;
        }
        
        // Different behavior for system vs user clients
        if (clientId_ == RecordClientId::System) {
            return StartSystemRecording();
        } else {
            return StartUserRecording();
        }
    }
    
    void StopRecording()
    {
        if (isActive_) {
            isActive_ = false;
            
            if (clientId_ == RecordClientId::System) {
                StopSystemRecording();
            } else {
                StopUserRecording();
            }
        }
    }
    
    bool IsSystemClient() const { return clientId_ == RecordClientId::System; }
    
private:
    bool StartSystemRecording()
    {
        // System-level recording initialization
        printf("Starting system recording for client %u\n", 
               static_cast<uint32_t>(clientId_));
        isActive_ = true;
        return true;
    }
    
    bool StartUserRecording()
    {
        // User-level recording initialization
        printf("Starting user recording '%s' for client %u\n", 
               sessionName_.c_str(), static_cast<uint32_t>(clientId_));
        isActive_ = true;
        return true;
    }
    
    void StopSystemRecording() { /* System cleanup */ }
    void StopUserRecording() { /* User cleanup */ }
};
```

### Client Permission and Access Control
```cpp
enum class ClientPermission
{
    Read      = 0x01,
    Write     = 0x02,
    Delete    = 0x04,
    System    = 0x08,
    Admin     = 0x10
};

class ClientAccessControl
{
    std::unordered_map<uint32_t, uint32_t> clientPermissions_;
    
public:
    void SetClientPermissions(RecordClientId clientId, uint32_t permissions)
    {
        if (clientId == RecordClientId::System) {
            // System client gets all permissions
            permissions |= static_cast<uint32_t>(ClientPermission::System);
            permissions |= static_cast<uint32_t>(ClientPermission::Admin);
        }
        
        clientPermissions_[static_cast<uint32_t>(clientId)] = permissions;
    }
    
    bool HasPermission(RecordClientId clientId, ClientPermission permission) const
    {
        if (clientId == RecordClientId::Invalid) {
            return false;
        }
        
        auto it = clientPermissions_.find(static_cast<uint32_t>(clientId));
        if (it == clientPermissions_.end()) {
            return false;
        }
        
        return (it->second & static_cast<uint32_t>(permission)) != 0;
    }
    
    bool CanAccessSystemResources(RecordClientId clientId) const
    {
        return HasPermission(clientId, ClientPermission::System) || 
               clientId == RecordClientId::System;
    }
    
    bool IsAdminClient(RecordClientId clientId) const
    {
        return HasPermission(clientId, ClientPermission::Admin) ||
               clientId == RecordClientId::System;
    }
};
```

### Client Statistics and Monitoring
```cpp
#include <atomic>
#include <mutex>

struct ClientStats
{
    std::atomic<uint64_t> recordingsStarted{0};
    std::atomic<uint64_t> recordingsCompleted{0};
    std::atomic<uint64_t> recordingsFailed{0};
    std::atomic<uint64_t> bytesRecorded{0};
    std::atomic<uint64_t> eventsRecorded{0};
};

class ClientMonitor
{
    std::unordered_map<uint32_t, std::unique_ptr<ClientStats>> clientStats_;
    mutable std::mutex statsMutex_;
    
public:
    void InitializeClientStats(RecordClientId clientId)
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        uint32_t id = static_cast<uint32_t>(clientId);
        clientStats_[id] = std::make_unique<ClientStats>();
    }
    
    void RecordStarted(RecordClientId clientId)
    {
        auto stats = GetClientStats(clientId);
        if (stats) {
            stats->recordingsStarted.fetch_add(1, std::memory_order_relaxed);
        }
    }
    
    void RecordCompleted(RecordClientId clientId, uint64_t bytesRecorded, uint64_t eventsRecorded)
    {
        auto stats = GetClientStats(clientId);
        if (stats) {
            stats->recordingsCompleted.fetch_add(1, std::memory_order_relaxed);
            stats->bytesRecorded.fetch_add(bytesRecorded, std::memory_order_relaxed);
            stats->eventsRecorded.fetch_add(eventsRecorded, std::memory_order_relaxed);
        }
    }
    
    void RecordFailed(RecordClientId clientId)
    {
        auto stats = GetClientStats(clientId);
        if (stats) {
            stats->recordingsFailed.fetch_add(1, std::memory_order_relaxed);
        }
    }
    
    ClientStats GetSnapshot(RecordClientId clientId) const
    {
        auto stats = GetClientStats(clientId);
        if (stats) {
            return *stats; // Atomic loads
        }
        return ClientStats{};
    }
    
private:
    ClientStats* GetClientStats(RecordClientId clientId) const
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        auto it = clientStats_.find(static_cast<uint32_t>(clientId));
        return (it != clientStats_.end()) ? it->second.get() : nullptr;
    }
};
```

### Utility Functions
```cpp
// Conversion and validation utilities
inline bool IsValidUserClient(RecordClientId clientId)
{
    return clientId != RecordClientId::Invalid &&
           clientId != RecordClientId::System &&
           clientId >= RecordClientId::Min &&
           clientId <= RecordClientId::Max;
}

inline bool IsSystemClient(RecordClientId clientId)
{
    return clientId == RecordClientId::System;
}

inline std::string ClientIdToString(RecordClientId clientId)
{
    switch (clientId) {
        case RecordClientId::Invalid:
            return "Invalid";
        case RecordClientId::System:
            return "System";
        default:
            return "Client-" + std::to_string(static_cast<uint32_t>(clientId));
    }
}

inline RecordClientId StringToClientId(std::string const& str)
{
    if (str == "Invalid") return RecordClientId::Invalid;
    if (str == "System") return RecordClientId::System;
    
    if (str.starts_with("Client-")) {
        try {
            uint32_t id = std::stoul(str.substr(7));
            if (id >= static_cast<uint32_t>(RecordClientId::Min) && 
                id <= static_cast<uint32_t>(RecordClientId::Max)) {
                return static_cast<RecordClientId>(id);
            }
        } catch (...) {
            // Invalid conversion
        }
    }
    
    return RecordClientId::Invalid;
}
```

## Important Notes

- Client ID 0 is reserved for invalid/uninitialized states
- Client ID UINT32_MAX is reserved for system-level operations
- User clients are assigned IDs in the range [1, UINT32_MAX-1]
- Client IDs should be managed centrally to avoid conflicts
- System clients have elevated privileges and access to internal operations
- Each recording session should be associated with a valid client ID for tracking and access control

## See Also

- [`RecordClient`](../IReplayEngine.h/struct-RecordClient.md) - Recording client metadata and user data
- [`SequenceId`](enum-SequenceId.md) - Timeline sequence identification
- [`ThreadId`](enum-ThreadId.md) - Thread identification within recordings
- [`ErrorCheckingLevel`](enum-ErrorCheckingLevel.md) - Error checking configuration for clients
