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
- Execute is triggered just before the instruction starts.
- CodeFetch is used to aggregate code usage; the size and exact hits are implementation-dependent.
- Overwrite triggers before a write/mismatch and provides the value being overwritten.
- DataMismatch/NewData/RedundantData classify data observations relative to previously seen data.

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
