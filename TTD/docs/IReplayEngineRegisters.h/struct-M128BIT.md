# M128BIT Structure

128-bit SIMD data type providing the foundation for SSE, SSE2, and AVX register operations in TTD register contexts. This structure enables efficient storage and manipulation of vector data across different processor architectures.

## Definition

```cpp
struct M128BIT {
    uint64_t Low;
    int64_t  High;
};
```

## Fields

- `Low` - Lower 64 bits of the 128-bit value (unsigned)
- `High` - Upper 64 bits of the 128-bit value (signed)

## Usage

### Basic Data Access
```cpp
void DisplayM128BIT(M128BIT const& value)
{
    printf("M128BIT: Low=0x%016llX, High=0x%016llX\n", 
           value.Low, static_cast<uint64_t>(value.High));
    
    // Access as bytes
    uint8_t const* bytes = reinterpret_cast<uint8_t const*>(&value);
    printf("Bytes: ");
    for (int i = 0; i < 16; ++i) {
        printf("%02X ", bytes[i]);
    }
    printf("\n");
}

void InitializeM128BIT(M128BIT& value, uint64_t low, uint64_t high)
{
    value.Low = low;
    value.High = static_cast<int64_t>(high);
}
```

### Floating-Point Interpretation
```cpp
// Interpret as different floating-point formats
void ProcessAsFloatingPoint(M128BIT const& simdReg)
{
    // As double precision (2 x 64-bit)
    double const* doubles = reinterpret_cast<double const*>(&simdReg);
    printf("Doubles: %f, %f\n", doubles[0], doubles[1]);
    
    // As single precision (4 x 32-bit)
    float const* floats = reinterpret_cast<float const*>(&simdReg);
    printf("Floats: %f, %f, %f, %f\n", 
           floats[0], floats[1], floats[2], floats[3]);
}
```

### Integer Interpretation
```cpp
// Interpret as different integer formats
struct M128BitView
{
    union {
        M128BIT raw;
        struct {
            uint64_t qwords[2];
        };
        struct {
            uint32_t dwords[4];
        };
        struct {
            uint16_t words[8];
        };
        struct {
            uint8_t bytes[16];
        };
        struct {
            int64_t sqwords[2];
        };
        struct {
            int32_t sdwords[4];
        };
        struct {
            int16_t swords[8];
        };
        struct {
            int8_t sbytes[16];
        };
    };
};

void ProcessAsIntegers(M128BIT const& simdReg)
{
    M128BitView view;
    view.raw = simdReg;
    
    printf("As unsigned integers:\n");
    printf("  QWords: 0x%016llX, 0x%016llX\n", view.qwords[0], view.qwords[1]);
    printf("  DWords: 0x%08X, 0x%08X, 0x%08X, 0x%08X\n", 
           view.dwords[0], view.dwords[1], view.dwords[2], view.dwords[3]);
    printf("  Words: ");
    for (int i = 0; i < 8; ++i) {
        printf("0x%04X ", view.words[i]);
    }
    printf("\n");
    
    printf("As signed integers:\n");
    printf("  SQWords: %lld, %lld\n", view.sqwords[0], view.sqwords[1]);
    printf("  SDWords: %d, %d, %d, %d\n", 
           view.sdwords[0], view.sdwords[1], view.sdwords[2], view.sdwords[3]);
}
```

### Vector Operations
```cpp
// Basic vector operations
M128BIT AddM128BIT(M128BIT const& a, M128BIT const& b)
{
    M128BIT result;
    result.Low = a.Low + b.Low;
    result.High = a.High + b.High;
    
    // Handle overflow from low to high
    if (result.Low < a.Low) {  // Overflow occurred
        result.High++;
    }
    
    return result;
}

M128BIT SubtractM128BIT(M128BIT const& a, M128BIT const& b)
{
    M128BIT result;
    result.High = a.High - b.High;
    
    if (a.Low < b.Low) {  // Borrow required
        result.High--;
        result.Low = (UINT64_MAX - b.Low) + a.Low + 1;
    } else {
        result.Low = a.Low - b.Low;
    }
    
    return result;
}

bool CompareM128BIT(M128BIT const& a, M128BIT const& b)
{
    return (a.Low == b.Low) && (a.High == b.High);
}

M128BIT BitwiseAND(M128BIT const& a, M128BIT const& b)
{
    M128BIT result;
    result.Low = a.Low & b.Low;
    result.High = a.High & b.High;
    return result;
}

M128BIT BitwiseOR(M128BIT const& a, M128BIT const& b)
{
    M128BIT result;
    result.Low = a.Low | b.Low;
    result.High = a.High | b.High;
    return result;
}

M128BIT BitwiseXOR(M128BIT const& a, M128BIT const& b)
{
    M128BIT result;
    result.Low = a.Low ^ b.Low;
    result.High = a.High ^ b.High;
    return result;
}
```

### SIMD Register Context Usage
```cpp
// Extract from XMM register
M128BIT ExtractFromXMMRegister(void const* xmmRegPtr)
{
    M128BIT result;
    memcpy(&result, xmmRegPtr, sizeof(M128BIT));
    return result;
}

// Set XMM register value
void SetXMMRegister(void* xmmRegPtr, M128BIT const& value)
{
    memcpy(xmmRegPtr, &value, sizeof(M128BIT));
}

// Process XMM registers from AMD64 context
void ProcessXMMRegisters(AMD64_CONTEXT const& context)
{
    printf("XMM Registers:\n");
    
    M128BIT const* xmmRegs[] = {
        &context.Xmm0, &context.Xmm1, &context.Xmm2, &context.Xmm3,
        &context.Xmm4, &context.Xmm5, &context.Xmm6, &context.Xmm7,
        &context.Xmm8, &context.Xmm9, &context.Xmm10, &context.Xmm11,
        &context.Xmm12, &context.Xmm13, &context.Xmm14, &context.Xmm15
    };
    
    for (int i = 0; i < 16; ++i) {
        printf("XMM%d: ", i);
        DisplayM128BIT(*xmmRegs[i]);
    }
}
```

### Conversion and Utility Functions
```cpp
// Convert to string representation
std::string M128BitToHexString(M128BIT const& value)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::uppercase;
    oss << std::setw(16) << static_cast<uint64_t>(value.High);
    oss << std::setw(16) << value.Low;
    return oss.str();
}

// Parse from string
bool ParseM128BitFromHex(std::string const& hexStr, M128BIT& result)
{
    if (hexStr.length() != 32) {
        return false;
    }
    
    try {
        std::string highStr = hexStr.substr(0, 16);
        std::string lowStr = hexStr.substr(16, 16);
        
        result.High = static_cast<int64_t>(std::stoull(highStr, nullptr, 16));
        result.Low = std::stoull(lowStr, nullptr, 16);
        
        return true;
    } catch (...) {
        return false;
    }
}

// Zero initialization
M128BIT ZeroM128BIT()
{
    M128BIT result;
    result.Low = 0;
    result.High = 0;
    return result;
}

// Check if zero
bool IsZeroM128BIT(M128BIT const& value)
{
    return (value.Low == 0) && (value.High == 0);
}

// Create from bytes
M128BIT CreateM128BitFromBytes(uint8_t const bytes[16])
{
    M128BIT result;
    memcpy(&result, bytes, 16);
    return result;
}

// Extract to bytes
void ExtractM128BitToBytes(M128BIT const& value, uint8_t bytes[16])
{
    memcpy(bytes, &value, 16);
}
```

### SIMD Instruction Emulation
```cpp
// Emulate common SIMD operations
struct SIMDOperations
{
    // Packed single-precision add
    static M128BIT AddPS(M128BIT const& a, M128BIT const& b)
    {
        float const* aFloats = reinterpret_cast<float const*>(&a);
        float const* bFloats = reinterpret_cast<float const*>(&b);
        
        M128BIT result;
        float* resultFloats = reinterpret_cast<float*>(&result);
        
        for (int i = 0; i < 4; ++i) {
            resultFloats[i] = aFloats[i] + bFloats[i];
        }
        
        return result;
    }
    
    // Packed double-precision multiply
    static M128BIT MulPD(M128BIT const& a, M128BIT const& b)
    {
        double const* aDoubles = reinterpret_cast<double const*>(&a);
        double const* bDoubles = reinterpret_cast<double const*>(&b);
        
        M128BIT result;
        double* resultDoubles = reinterpret_cast<double*>(&result);
        
        resultDoubles[0] = aDoubles[0] * bDoubles[0];
        resultDoubles[1] = aDoubles[1] * bDoubles[1];
        
        return result;
    }
    
    // Shuffle (simplified version)
    static M128BIT Shuffle(M128BIT const& a, M128BIT const& b, uint8_t imm)
    {
        uint32_t const* aDwords = reinterpret_cast<uint32_t const*>(&a);
        uint32_t const* bDwords = reinterpret_cast<uint32_t const*>(&b);
        
        M128BIT result;
        uint32_t* resultDwords = reinterpret_cast<uint32_t*>(&result);
        
        resultDwords[0] = aDwords[imm & 0x3];
        resultDwords[1] = aDwords[(imm >> 2) & 0x3];
        resultDwords[2] = bDwords[(imm >> 4) & 0x3];
        resultDwords[3] = bDwords[(imm >> 6) & 0x3];
        
        return result;
    }
};
```

### Register Analysis Tools
```cpp
// Analyze register content patterns
struct M128BitAnalysis
{
    bool isAllZeros;
    bool isAllOnes;
    bool hasFloatingPointPattern;
    bool hasIntegerPattern;
    size_t nonZeroBytes;
    double entropyScore;
};

M128BitAnalysis AnalyzeM128Bit(M128BIT const& value)
{
    M128BitAnalysis analysis{};
    
    uint8_t const* bytes = reinterpret_cast<uint8_t const*>(&value);
    
    // Check for all zeros/ones
    analysis.isAllZeros = IsZeroM128BIT(value);
    analysis.isAllOnes = (value.Low == UINT64_MAX) && (value.High == -1);
    
    // Count non-zero bytes
    analysis.nonZeroBytes = 0;
    for (int i = 0; i < 16; ++i) {
        if (bytes[i] != 0) {
            analysis.nonZeroBytes++;
        }
    }
    
    // Basic pattern detection
    float const* floats = reinterpret_cast<float const*>(&value);
    bool validFloats = true;
    for (int i = 0; i < 4; ++i) {
        if (!std::isfinite(floats[i])) {
            validFloats = false;
            break;
        }
    }
    analysis.hasFloatingPointPattern = validFloats;
    
    // Simple entropy calculation (simplified)
    std::map<uint8_t, int> byteFreq;
    for (int i = 0; i < 16; ++i) {
        byteFreq[bytes[i]]++;
    }
    
    analysis.entropyScore = 0.0;
    for (auto const& pair : byteFreq) {
        double prob = static_cast<double>(pair.second) / 16.0;
        if (prob > 0) {
            analysis.entropyScore -= prob * log2(prob);
        }
    }
    
    return analysis;
}

void DisplayM128BitAnalysis(M128BIT const& value)
{
    auto analysis = AnalyzeM128Bit(value);
    
    printf("M128BIT Analysis:\n");
    printf("  All zeros: %s\n", analysis.isAllZeros ? "Yes" : "No");
    printf("  All ones: %s\n", analysis.isAllOnes ? "Yes" : "No");
    printf("  Non-zero bytes: %zu/16\n", analysis.nonZeroBytes);
    printf("  Entropy: %.2f bits\n", analysis.entropyScore);
    printf("  Likely floating-point: %s\n", 
           analysis.hasFloatingPointPattern ? "Yes" : "No");
}
```

### Memory-Efficient Operations
```cpp
// In-place operations to avoid copying
void InPlaceAND(M128BIT& target, M128BIT const& operand)
{
    target.Low &= operand.Low;
    target.High &= operand.High;
}

void InPlaceOR(M128BIT& target, M128BIT const& operand)
{
    target.Low |= operand.Low;
    target.High |= operand.High;
}

void InPlaceXOR(M128BIT& target, M128BIT const& operand)
{
    target.Low ^= operand.Low;
    target.High ^= operand.High;
}

void InPlaceNOT(M128BIT& target)
{
    target.Low = ~target.Low;
    target.High = ~target.High;
}

// Shift operations
M128BIT ShiftLeft(M128BIT const& value, int bits)
{
    if (bits >= 128) {
        return ZeroM128BIT();
    }
    
    M128BIT result = value;
    
    if (bits >= 64) {
        result.High = static_cast<int64_t>(result.Low) << (bits - 64);
        result.Low = 0;
    } else if (bits > 0) {
        uint64_t carry = result.Low >> (64 - bits);
        result.Low <<= bits;
        result.High = (result.High << bits) | static_cast<int64_t>(carry);
    }
    
    return result;
}

M128BIT ShiftRight(M128BIT const& value, int bits)
{
    if (bits >= 128) {
        return ZeroM128BIT();
    }
    
    M128BIT result = value;
    
    if (bits >= 64) {
        result.Low = static_cast<uint64_t>(result.High) >> (bits - 64);
        result.High = 0;
    } else if (bits > 0) {
        uint64_t carry = (static_cast<uint64_t>(result.High) & ((1ULL << bits) - 1)) << (64 - bits);
        result.High >>= bits;
        result.Low = (result.Low >> bits) | carry;
    }
    
    return result;
}
```

## Important Notes

- M128BIT serves as the foundation for larger SIMD types (M256BIT, M512BIT)
- The `High` field is signed to facilitate sign extension operations
- Byte order follows little-endian format on x86/x64 platforms
- Direct memory manipulation requires careful attention to alignment
- Floating-point interpretation depends on the context of usage
- Vector operations may require architecture-specific optimization
- Register values represent snapshots at specific execution points

## See Also

- [`M256BIT`](struct-M256BIT.md) - 256-bit SIMD data type built from M128BIT components
- [`M512BIT`](struct-M512BIT.md) - 512-bit SIMD data type for advanced vector operations
- [`AMD64_CONTEXT`](struct-AMD64_CONTEXT.md) - Uses M128BIT for XMM registers
- [`VECTOR_128BIT_REGISTERS`](struct-VECTOR_128BIT_REGISTERS.md) - Array of M128BIT registers
