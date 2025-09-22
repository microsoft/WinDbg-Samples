# SelectRegisters Flags

Bitmask specifying which register groups to access.

## Common flags
- General — General-purpose registers
- FloatingPoint — Floating point/SSE registers
- Debug — Debug registers
- System — System/segment-specific registers

Flags can be combined with bitwise OR.

## Usage
```cpp
SelectRegisters mask = SelectRegisters::General | SelectRegisters::FloatingPoint;
RegisterSet regs = view->GetRegisters(mask);
```

## Notes
- Exact flag names and availability depend on architecture.
- Using a narrower mask can improve performance by fetching fewer registers.
