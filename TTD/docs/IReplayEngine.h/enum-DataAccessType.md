# DataAccessType Enumeration

Describes the type of memory access observed by the replay engine.

## Definition
```cpp
enum class DataAccessType : uint8_t
{
    Read          = 0,
    Write         = 1,
    Execute       = 2,
    CodeFetch     = 3,
    Overwrite     = 4,
    DataMismatch  = 5,
    NewData       = 6,
    RedundantData = 7,
};

constexpr char const* GetDataAccessTypeName(DataAccessType type);
constexpr wchar_t const* GetDataAccessTypeNameW(DataAccessType type);
constexpr bool IsDataAccessBeforeInstruction(DataAccessType type);
```

Notes:
- Execute - Triggered just before the instruction starts.
- CodeFetch - Used to aggregate code usage; the size and exact hits are implementation-dependent.
- Overwrite - Triggered before a write/mismatch, allowing the client to inspect the value before it is overwritten.
- DataMismatch - The memory cache predicted the wrong value.
- NewData - First time seeing data at this address.
- RedundantData - Data read from trace file matches memory cache.

## Related mask type
```cpp
enum class DataAccessMask : uint8_t
{
    Read          = /* 1 << DataAccessType::Read */          ,
    Write         = /* 1 << DataAccessType::Write */         ,
    Execute       = /* 1 << DataAccessType::Execute */       ,
    CodeFetch     = /* 1 << DataAccessType::CodeFetch */     ,
    Overwrite     = /* 1 << DataAccessType::Overwrite */     ,
    DataMismatch  = /* 1 << DataAccessType::DataMismatch */  ,
    NewData       = /* 1 << DataAccessType::NewData */       ,
    RedundantData = /* 1 << DataAccessType::RedundantData */ ,

    None      = 0,
    ReadWrite = Read | Write,
    All       = Read | Write | Execute | CodeFetch | Overwrite | DataMismatch | NewData | RedundantData,
};
```

## See also
- [MemoryWatchpointData](struct-MemoryWatchpointData.md)
