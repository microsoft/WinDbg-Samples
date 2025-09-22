# X86_FXSAVE_FORMAT Structure

Complete x86 processor floating-point and SIMD register save area format used by the FXSAVE/FXRSTOR instructions for preserving extended processor state in TTD register contexts.

## Definition

```cpp
struct X86_FXSAVE_FORMAT {
    USHORT ControlWord;
    USHORT StatusWord;
    UCHAR TagWord;
    UCHAR Reserved1;
    USHORT ErrorOpcode;
    ULONG ErrorOffset;
    USHORT ErrorSelector;
    USHORT Reserved2;
    ULONG DataOffset;
    USHORT DataSelector;
    USHORT Reserved3;
    ULONG MxCsr;
    ULONG MxCsr_Mask;
    M128BIT FloatingPointRegisters[8];
    M128BIT XmmRegisters[8];
    UCHAR Reserved4[224];
};
```

## Fields

### Floating-Point Control Fields
- `ControlWord` - x87 FPU control word (exception masks, precision, rounding)
- `StatusWord` - x87 FPU status word (exception flags, condition codes, stack pointer)
- `TagWord` - x87 FPU tag word (register validity, compressed format)
- `Reserved1` - Alignment padding (1 byte)

### Error Information
- `ErrorOpcode` - Last floating-point instruction opcode that caused an exception
- `ErrorOffset` - Offset of last floating-point instruction that caused an exception
- `ErrorSelector` - Segment selector for last floating-point instruction error
- `Reserved2` - Alignment padding (2 bytes)
- `DataOffset` - Offset of last floating-point operand that caused an exception
- `DataSelector` - Segment selector for last floating-point operand error
- `Reserved3` - Alignment padding (2 bytes)

### SSE Control Fields
- `MxCsr` - SSE control and status register
- `MxCsr_Mask` - Mask indicating which MXCSR bits are writable

### Register Arrays
- `FloatingPointRegisters[8]` - x87 FPU stack registers (ST0-ST7) in 128-bit format
- `XmmRegisters[8]` - SSE XMM registers (XMM0-XMM7) in 128-bit format
- `Reserved4[224]` - Reserved space for future extensions and alignment

## Usage

### Basic State Access
```cpp
void DisplayFXSAVEFormat(X86_FXSAVE_FORMAT const& fxsave)
{
    printf("X86 FXSAVE Format:\n");
    
    // Control and status
    printf("  Control Word: 0x%04X\n", fxsave.ControlWord);
    printf("  Status Word:  0x%04X\n", fxsave.StatusWord);
    printf("  Tag Word:     0x%02X\n", fxsave.TagWord);
    printf("  MXCSR:        0x%08X\n", fxsave.MxCsr);
    printf("  MXCSR Mask:   0x%08X\n", fxsave.MxCsr_Mask);
    
    // Error information
    if (fxsave.ErrorOpcode != 0) {
        printf("  Last Error:\n");
        printf("    Opcode:   0x%04X\n", fxsave.ErrorOpcode);
        printf("    Offset:   0x%08X\n", fxsave.ErrorOffset);
        printf("    Selector: 0x%04X\n", fxsave.ErrorSelector);
        printf("    Data Off: 0x%08X\n", fxsave.DataOffset);
        printf("    Data Sel: 0x%04X\n", fxsave.DataSelector);
    }
}

void InitializeFXSAVE(X86_FXSAVE_FORMAT& fxsave)
{
    memset(&fxsave, 0, sizeof(X86_FXSAVE_FORMAT));
    
    // Set default control word (mask all exceptions, double precision, round to nearest)
    fxsave.ControlWord = 0x037F;
    
    // Set default MXCSR (mask all exceptions, round to nearest, flush to zero off)
    fxsave.MxCsr = 0x1F80;
    fxsave.MxCsr_Mask = 0xFFBF;  // All bits writable except reserved
    
    // Initialize tag word (all registers empty)
    fxsave.TagWord = 0xFF;
}
```

### Floating-Point Register Analysis
```cpp
// Decode x87 FPU control word
struct FPUControlWord
{
    union {
        USHORT raw;
        struct {
            USHORT InvalidOperation : 1;    // Bit 0: IM
            USHORT DenormalOperand : 1;     // Bit 1: DM
            USHORT ZeroDivide : 1;          // Bit 2: ZM
            USHORT Overflow : 1;            // Bit 3: OM
            USHORT Underflow : 1;           // Bit 4: UM
            USHORT Precision : 1;           // Bit 5: PM
            USHORT Reserved1 : 2;           // Bits 6-7
            USHORT PrecisionControl : 2;    // Bits 8-9: PC
            USHORT RoundingControl : 2;     // Bits 10-11: RC
            USHORT InfinityControl : 1;     // Bit 12: X (deprecated)
            USHORT Reserved2 : 3;           // Bits 13-15
        };
    };
};

void AnalyzeFPUControlWord(USHORT controlWord)
{
    FPUControlWord cw;
    cw.raw = controlWord;
    
    printf("FPU Control Word Analysis (0x%04X):\n", controlWord);
    printf("  Exception Masks:\n");
    printf("    Invalid Operation: %s\n", cw.InvalidOperation ? "Masked" : "Unmasked");
    printf("    Denormal Operand:  %s\n", cw.DenormalOperand ? "Masked" : "Unmasked");
    printf("    Zero Divide:       %s\n", cw.ZeroDivide ? "Masked" : "Unmasked");
    printf("    Overflow:          %s\n", cw.Overflow ? "Masked" : "Unmasked");
    printf("    Underflow:         %s\n", cw.Underflow ? "Masked" : "Unmasked");
    printf("    Precision:         %s\n", cw.Precision ? "Masked" : "Unmasked");
    
    printf("  Precision Control: ");
    switch (cw.PrecisionControl) {
        case 0: printf("Single (24-bit)\n"); break;
        case 1: printf("Reserved\n"); break;
        case 2: printf("Double (53-bit)\n"); break;
        case 3: printf("Extended (64-bit)\n"); break;
    }
    
    printf("  Rounding Control: ");
    switch (cw.RoundingControl) {
        case 0: printf("Round to nearest\n"); break;
        case 1: printf("Round down (toward -∞)\n"); break;
        case 2: printf("Round up (toward +∞)\n"); break;
        case 3: printf("Round toward zero (truncate)\n"); break;
    }
}

// Decode x87 FPU status word
struct FPUStatusWord
{
    union {
        USHORT raw;
        struct {
            USHORT InvalidOperation : 1;    // Bit 0: IE
            USHORT DenormalOperand : 1;     // Bit 1: DE
            USHORT ZeroDivide : 1;          // Bit 2: ZE
            USHORT Overflow : 1;            // Bit 3: OE
            USHORT Underflow : 1;           // Bit 4: UE
            USHORT Precision : 1;           // Bit 5: PE
            USHORT StackFault : 1;          // Bit 6: SF
            USHORT ErrorSummary : 1;        // Bit 7: ES
            USHORT ConditionC0 : 1;         // Bit 8: C0
            USHORT ConditionC1 : 1;         // Bit 9: C1
            USHORT ConditionC2 : 1;         // Bit 10: C2
            USHORT StackPointer : 3;        // Bits 11-13: TOP
            USHORT ConditionC3 : 1;         // Bit 14: C3
            USHORT Busy : 1;                // Bit 15: B
        };
    };
};

void AnalyzeFPUStatusWord(USHORT statusWord)
{
    FPUStatusWord sw;
    sw.raw = statusWord;
    
    printf("FPU Status Word Analysis (0x%04X):\n", statusWord);
    printf("  Exception Flags:\n");
    printf("    Invalid Operation: %s\n", sw.InvalidOperation ? "Set" : "Clear");
    printf("    Denormal Operand:  %s\n", sw.DenormalOperand ? "Set" : "Clear");
    printf("    Zero Divide:       %s\n", sw.ZeroDivide ? "Set" : "Clear");
    printf("    Overflow:          %s\n", sw.Overflow ? "Set" : "Clear");
    printf("    Underflow:         %s\n", sw.Underflow ? "Set" : "Clear");
    printf("    Precision:         %s\n", sw.Precision ? "Set" : "Clear");
    printf("    Stack Fault:       %s\n", sw.StackFault ? "Set" : "Clear");
    printf("    Error Summary:     %s\n", sw.ErrorSummary ? "Set" : "Clear");
    
    printf("  Stack Pointer (TOP): %d (ST%d is top)\n", sw.StackPointer, sw.StackPointer);
    printf("  Condition Codes: C3=%d C2=%d C1=%d C0=%d\n", 
           sw.ConditionC3, sw.ConditionC2, sw.ConditionC1, sw.ConditionC0);
    printf("  Busy: %s\n", sw.Busy ? "Yes" : "No");
}
```

### SSE Control and Status
```cpp
// Decode MXCSR register
struct MXCSRRegister
{
    union {
        ULONG raw;
        struct {
            ULONG InvalidOperation : 1;     // Bit 0: IE
            ULONG DenormalOperand : 1;      // Bit 1: DE
            ULONG ZeroDivide : 1;          // Bit 2: ZE
            ULONG Overflow : 1;            // Bit 3: OE
            ULONG Underflow : 1;           // Bit 4: UE
            ULONG Precision : 1;           // Bit 5: PE
            ULONG DenormalsAreZero : 1;    // Bit 6: DAZ
            ULONG InvalidMask : 1;         // Bit 7: IM
            ULONG DenormalMask : 1;        // Bit 8: DM
            ULONG ZeroDivideMask : 1;      // Bit 9: ZM
            ULONG OverflowMask : 1;        // Bit 10: OM
            ULONG UnderflowMask : 1;       // Bit 11: UM
            ULONG PrecisionMask : 1;       // Bit 12: PM
            ULONG RoundingControl : 2;     // Bits 13-14: RC
            ULONG FlushToZero : 1;         // Bit 15: FZ
            ULONG Reserved : 16;           // Bits 16-31
        };
    };
};

void AnalyzeMXCSR(ULONG mxcsr)
{
    MXCSRRegister mx;
    mx.raw = mxcsr;
    
    printf("MXCSR Analysis (0x%08X):\n", mxcsr);
    printf("  Exception Flags:\n");
    printf("    Invalid Operation: %s\n", mx.InvalidOperation ? "Set" : "Clear");
    printf("    Denormal Operand:  %s\n", mx.DenormalOperand ? "Set" : "Clear");
    printf("    Zero Divide:       %s\n", mx.ZeroDivide ? "Set" : "Clear");
    printf("    Overflow:          %s\n", mx.Overflow ? "Set" : "Clear");
    printf("    Underflow:         %s\n", mx.Underflow ? "Set" : "Clear");
    printf("    Precision:         %s\n", mx.Precision ? "Set" : "Clear");
    
    printf("  Exception Masks:\n");
    printf("    Invalid:    %s\n", mx.InvalidMask ? "Masked" : "Unmasked");
    printf("    Denormal:   %s\n", mx.DenormalMask ? "Masked" : "Unmasked");
    printf("    Zero Div:   %s\n", mx.ZeroDivideMask ? "Masked" : "Unmasked");
    printf("    Overflow:   %s\n", mx.OverflowMask ? "Masked" : "Unmasked");
    printf("    Underflow:  %s\n", mx.UnderflowMask ? "Masked" : "Unmasked");
    printf("    Precision:  %s\n", mx.PrecisionMask ? "Masked" : "Unmasked");
    
    printf("  Control Flags:\n");
    printf("    Denormals Are Zero: %s\n", mx.DenormalsAreZero ? "Enabled" : "Disabled");
    printf("    Flush To Zero:      %s\n", mx.FlushToZero ? "Enabled" : "Disabled");
    
    printf("  Rounding Control: ");
    switch (mx.RoundingControl) {
        case 0: printf("Round to nearest\n"); break;
        case 1: printf("Round down (toward -∞)\n"); break;
        case 2: printf("Round up (toward +∞)\n"); break;
        case 3: printf("Round toward zero (truncate)\n"); break;
    }
}
```

### Register State Processing
```cpp
// Process x87 FPU registers
void ProcessFPURegisters(X86_FXSAVE_FORMAT const& fxsave)
{
    printf("x87 FPU Registers:\n");
    
    FPUStatusWord sw;
    sw.raw = fxsave.StatusWord;
    int top = sw.StackPointer;
    
    for (int i = 0; i < 8; ++i) {
        int physicalReg = i;
        int logicalReg = (i + top) % 8;
        
        printf("  ST(%d) [R%d]: ", logicalReg, physicalReg);
        
        // Check tag for register state
        int tagBits = (fxsave.TagWord >> (physicalReg * 2)) & 3;
        switch (tagBits) {
            case 0: printf("Valid - "); break;
            case 1: printf("Zero - "); break;
            case 2: printf("Special - "); break;
            case 3: printf("Empty\n"); continue;
        }
        
        // Display as 80-bit extended precision
        M128BIT const& reg = fxsave.FloatingPointRegisters[physicalReg];
        
        // Extract 80-bit format: 64-bit mantissa + 16-bit sign/exponent
        uint64_t mantissa = reg.Low;
        uint16_t signExp = static_cast<uint16_t>(reg.High);
        
        bool sign = (signExp & 0x8000) != 0;
        uint16_t exponent = signExp & 0x7FFF;
        
        printf("%s", sign ? "-" : "+");
        
        if (exponent == 0) {
            if (mantissa == 0) {
                printf("0.0");
            } else {
                printf("Denormal (0x%016llX)", mantissa);
            }
        } else if (exponent == 0x7FFF) {
            if (mantissa == 0x8000000000000000ULL) {
                printf("Infinity");
            } else {
                printf("NaN (0x%016llX)", mantissa);
            }
        } else {
            // Normal number - convert to double for display
            double value = ConvertFrom80Bit(signExp, mantissa);
            printf("%.17g (raw: 0x%04X_%016llX)", value, signExp, mantissa);
        }
        printf("\n");
    }
}

// Process SSE XMM registers
void ProcessXMMRegisters(X86_FXSAVE_FORMAT const& fxsave)
{
    printf("SSE XMM Registers:\n");
    
    for (int i = 0; i < 8; ++i) {
        printf("  XMM%d: ", i);
        DisplayM128BIT(fxsave.XmmRegisters[i]);
    }
}

// Helper function to convert 80-bit to double
double ConvertFrom80Bit(uint16_t signExp, uint64_t mantissa)
{
    // Simplified conversion - real implementation would handle all edge cases
    bool sign = (signExp & 0x8000) != 0;
    int exponent = (signExp & 0x7FFF) - 16383;  // Remove bias
    
    if (exponent < -1022) return sign ? -0.0 : 0.0;  // Underflow
    if (exponent > 1023) return sign ? -INFINITY : INFINITY;  // Overflow
    
    // Extract mantissa (assume normalized)
    double mantissaDouble = static_cast<double>(mantissa >> 11) / (1ULL << 53);
    double result = ldexp(mantissaDouble, exponent);
    
    return sign ? -result : result;
}
```

### State Validation and Diagnostics
```cpp
// Validate FXSAVE format consistency
struct FXSAVEValidation
{
    bool isValid;
    bool hasExceptions;
    bool hasNaNs;
    bool hasInfinities;
    bool stackConsistent;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

FXSAVEValidation ValidateFXSAVE(X86_FXSAVE_FORMAT const& fxsave)
{
    FXSAVEValidation result{};
    result.isValid = true;
    
    // Check for reserved field violations
    if (fxsave.Reserved1 != 0) {
        result.warnings.push_back("Reserved1 field is non-zero");
    }
    
    if (fxsave.Reserved2 != 0) {
        result.warnings.push_back("Reserved2 field is non-zero");
    }
    
    if (fxsave.Reserved3 != 0) {
        result.warnings.push_back("Reserved3 field is non-zero");
    }
    
    // Check MXCSR validity
    if ((fxsave.MxCsr & ~fxsave.MxCsr_Mask) != 0) {
        result.errors.push_back("MXCSR has bits set that are masked as unwritable");
        result.isValid = false;
    }
    
    // Check for exception flags
    FPUStatusWord sw;
    sw.raw = fxsave.StatusWord;
    result.hasExceptions = sw.ErrorSummary || sw.StackFault ||
                          sw.InvalidOperation || sw.DenormalOperand ||
                          sw.ZeroDivide || sw.Overflow ||
                          sw.Underflow || sw.Precision;
    
    MXCSRRegister mx;
    mx.raw = fxsave.MxCsr;
    if (!result.hasExceptions) {
        result.hasExceptions = mx.InvalidOperation || mx.DenormalOperand ||
                              mx.ZeroDivide || mx.Overflow ||
                              mx.Underflow || mx.Precision;
    }
    
    // Check FPU stack consistency
    int top = sw.StackPointer;
    int emptyCount = 0;
    int validCount = 0;
    
    for (int i = 0; i < 8; ++i) {
        int tagBits = (fxsave.TagWord >> (i * 2)) & 3;
        if (tagBits == 3) {
            emptyCount++;
        } else {
            validCount++;
            
            // Check for special values in valid registers
            M128BIT const& reg = fxsave.FloatingPointRegisters[i];
            uint16_t signExp = static_cast<uint16_t>(reg.High);
            uint64_t mantissa = reg.Low;
            
            if ((signExp & 0x7FFF) == 0x7FFF) {
                if (mantissa == 0x8000000000000000ULL) {
                    result.hasInfinities = true;
                } else {
                    result.hasNaNs = true;
                }
            }
        }
    }
    
    result.stackConsistent = (emptyCount + validCount == 8);
    if (!result.stackConsistent) {
        result.warnings.push_back("FPU stack tag consistency issue");
    }
    
    // Check XMM registers for special values
    for (int i = 0; i < 8; ++i) {
        float const* floats = reinterpret_cast<float const*>(&fxsave.XmmRegisters[i]);
        for (int j = 0; j < 4; ++j) {
            if (std::isnan(floats[j])) {
                result.hasNaNs = true;
            } else if (std::isinf(floats[j])) {
                result.hasInfinities = true;
            }
        }
    }
    
    return result;
}

void DisplayFXSAVEValidation(X86_FXSAVE_FORMAT const& fxsave)
{
    auto validation = ValidateFXSAVE(fxsave);
    
    printf("FXSAVE Validation:\n");
    printf("  Overall Valid: %s\n", validation.isValid ? "Yes" : "No");
    printf("  Has Exceptions: %s\n", validation.hasExceptions ? "Yes" : "No");
    printf("  Has NaNs: %s\n", validation.hasNaNs ? "Yes" : "No");
    printf("  Has Infinities: %s\n", validation.hasInfinities ? "Yes" : "No");
    printf("  Stack Consistent: %s\n", validation.stackConsistent ? "Yes" : "No");
    
    if (!validation.warnings.empty()) {
        printf("  Warnings:\n");
        for (auto const& warning : validation.warnings) {
            printf("    - %s\n", warning.c_str());
        }
    }
    
    if (!validation.errors.empty()) {
        printf("  Errors:\n");
        for (auto const& error : validation.errors) {
            printf("    - %s\n", error.c_str());
        }
    }
}
```

### Memory Operations
```cpp
// Save/restore operations
void SaveFXSAVEState(X86_FXSAVE_FORMAT const& source, void* buffer)
{
    // Ensure proper alignment for FXSAVE format
    if (reinterpret_cast<uintptr_t>(buffer) % 16 != 0) {
        throw std::invalid_argument("Buffer must be 16-byte aligned for FXSAVE format");
    }
    
    memcpy(buffer, &source, sizeof(X86_FXSAVE_FORMAT));
}

bool RestoreFXSAVEState(void const* buffer, X86_FXSAVE_FORMAT& target)
{
    if (reinterpret_cast<uintptr_t>(buffer) % 16 != 0) {
        return false;
    }
    
    memcpy(&target, buffer, sizeof(X86_FXSAVE_FORMAT));
    
    // Validate restored state
    auto validation = ValidateFXSAVE(target);
    return validation.isValid;
}

// Compare states for debugging
bool CompareFXSAVEStates(X86_FXSAVE_FORMAT const& a, X86_FXSAVE_FORMAT const& b, 
                        bool ignoreReservedFields = true)
{
    // Component-wise comparison for better debugging
    bool identical = true;
    
    if (a.ControlWord != b.ControlWord) {
        printf("Control Word differs: 0x%04X vs 0x%04X\n", a.ControlWord, b.ControlWord);
        identical = false;
    }
    
    if (a.StatusWord != b.StatusWord) {
        printf("Status Word differs: 0x%04X vs 0x%04X\n", a.StatusWord, b.StatusWord);
        identical = false;
    }
    
    if (a.TagWord != b.TagWord) {
        printf("Tag Word differs: 0x%02X vs 0x%02X\n", a.TagWord, b.TagWord);
        identical = false;
    }
    
    if (a.MxCsr != b.MxCsr) {
        printf("MXCSR differs: 0x%08X vs 0x%08X\n", a.MxCsr, b.MxCsr);
        identical = false;
    }
    
    // Compare register arrays
    for (int i = 0; i < 8; ++i) {
        if (!CompareM128BIT(a.FloatingPointRegisters[i], b.FloatingPointRegisters[i])) {
            printf("FPU Register %d differs\n", i);
            identical = false;
        }
        
        if (!CompareM128BIT(a.XmmRegisters[i], b.XmmRegisters[i])) {
            printf("XMM Register %d differs\n", i);
            identical = false;
        }
    }
    
    return identical;
}
```

## Important Notes

- FXSAVE format requires 16-byte memory alignment for proper operation
- Tag word uses compressed format (2 bits per register) unlike older FSAVE format
- MXCSR controls both exception handling and performance optimizations
- Error information fields capture the last floating-point exception context
- Reserved fields should be zero but may contain undefined values
- XMM registers store full 128-bit SSE data including packed formats
- FPU registers use 80-bit extended precision internally
- State represents snapshot at specific execution point in trace

## See Also

- [`M128BIT`](struct-M128BIT.md) - 128-bit SIMD data type used for registers
- [`X86_FLOATING_SAVE_AREA`](struct-X86_FLOATING_SAVE_AREA.md) - Legacy floating-point save format
- [`X86_NT5_CONTEXT`](struct-X86_NT5_CONTEXT.md) - Complete x86 processor context including FXSAVE
- [`AMD64_XMM_SAVE_AREA32`](struct-AMD64_XMM_SAVE_AREA32.md) - AMD64 equivalent structure
