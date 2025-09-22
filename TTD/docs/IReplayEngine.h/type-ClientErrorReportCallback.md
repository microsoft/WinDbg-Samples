# ClientErrorReportCallback Type (Deprecated)

Legacy callback used for client error reporting.

## Typical signature
```cpp
using ClientErrorReportCallback = void(*)(unsigned code, wchar_t const* message);
```

## Notes
- Deprecated; prefer modern error reporting facilities where available.
