# M256BIT Structure

256-bit SIMD data type for AVX (Advanced Vector Extensions) register operations, providing double the width of SSE registers for enhanced parallel processing capabilities in TTD register contexts.

## Definition

```cpp
struct M256BIT {
    M128BIT Low;
    M128BIT High;
};
```

## Fields

- `Low` - Lower 128 bits of the 256-bit value
- `High` - Upper 128 bits of the 256-bit value

## Usage

### Basic Data Access
```cpp
void DisplayM256BIT(M256BIT const& value)
{
    printf("M256BIT:\n");
    printf("  Low:  0x%016llX_%016llX\n", 
           static_cast<uint64_t>(value.Low.High), value.Low.Low);
    printf("  High: 0x%016llX_%016llX\n", 
           static_cast<uint64_t>(value.High.High), value.High.Low);
    
    // Access as 32 bytes
    uint8_t const* bytes = reinterpret_cast<uint8_t const*>(&value);
    printf("  Bytes: ");
    for (int i = 0; i < 32; ++i) {
        printf("%02X", bytes[i]);
        if ((i + 1) % 4 == 0) printf(" ");
    }
    printf("\n");
}

void InitializeM256BIT(M256BIT& value, M128BIT const& low, M128BIT const& high)
{
    value.Low = low;
    value.High = high;
}

M256BIT CreateM256BIT(uint64_t ll, uint64_t lh, uint64_t hl, uint64_t hh)
{
    M256BIT result;
    result.Low.Low = ll;
    result.Low.High = static_cast<int64_t>(lh);
    result.High.Low = hl;
    result.High.High = static_cast<int64_t>(hh);
    return result;
}
```

### Floating-Point Interpretation
```cpp
// Interpret as different floating-point formats
void ProcessAsFloatingPoint(M256BIT const& avxReg)
{
    // As double precision (4 x 64-bit)
    double const* doubles = reinterpret_cast<double const*>(&avxReg);
    printf("Doubles: ");
    for (int i = 0; i < 4; ++i) {
        printf("%f ", doubles[i]);
    }
    printf("\n");
    
    // As single precision (8 x 32-bit)
    float const* floats = reinterpret_cast<float const*>(&avxReg);
    printf("Floats: ");
    for (int i = 0; i < 8; ++i) {
        printf("%f ", floats[i]);
    }
    printf("\n");
}

// AVX packed operations simulation
struct AVXOperations
{
    // Packed single-precision add (8 elements)
    static M256BIT AddPS(M256BIT const& a, M256BIT const& b)
    {
        float const* aFloats = reinterpret_cast<float const*>(&a);
        float const* bFloats = reinterpret_cast<float const*>(&b);
        
        M256BIT result;
        float* resultFloats = reinterpret_cast<float*>(&result);
        
        for (int i = 0; i < 8; ++i) {
            resultFloats[i] = aFloats[i] + bFloats[i];
        }
        
        return result;
    }
    
    // Packed double-precision multiply (4 elements)
    static M256BIT MulPD(M256BIT const& a, M256BIT const& b)
    {
        double const* aDoubles = reinterpret_cast<double const*>(&a);
        double const* bDoubles = reinterpret_cast<double const*>(&b);
        
        M256BIT result;
        double* resultDoubles = reinterpret_cast<double*>(&result);
        
        for (int i = 0; i < 4; ++i) {
            resultDoubles[i] = aDoubles[i] * bDoubles[i];
        }
        
        return result;
    }
    
    // Horizontal add
    static M256BIT HAddPS(M256BIT const& a, M256BIT const& b)
    {
        float const* aFloats = reinterpret_cast<float const*>(&a);
        float const* bFloats = reinterpret_cast<float const*>(&b);
        
        M256BIT result;
        float* resultFloats = reinterpret_cast<float*>(&result);
        
        // Low 128-bit lane
        resultFloats[0] = aFloats[0] + aFloats[1];
        resultFloats[1] = aFloats[2] + aFloats[3];
        resultFloats[2] = bFloats[0] + bFloats[1];
        resultFloats[3] = bFloats[2] + bFloats[3];
        
        // High 128-bit lane
        resultFloats[4] = aFloats[4] + aFloats[5];
        resultFloats[5] = aFloats[6] + aFloats[7];
        resultFloats[6] = bFloats[4] + bFloats[5];
        resultFloats[7] = bFloats[6] + bFloats[7];
        
        return result;
    }
};
```

### Integer Interpretation
```cpp
// Comprehensive integer view
struct M256BitView
{
    union {
        M256BIT raw;
        struct {
            uint64_t qwords[4];
        };
        struct {
            uint32_t dwords[8];
        };
        struct {
            uint16_t words[16];
        };
        struct {
            uint8_t bytes[32];
        };
        struct {
            int64_t sqwords[4];
        };
        struct {
            int32_t sdwords[8];
        };
        struct {
            int16_t swords[16];
        };
        struct {
            int8_t sbytes[32];
        };
    };
};

void ProcessAsIntegers(M256BIT const& avxReg)
{
    M256BitView view;
    view.raw = avxReg;
    
    printf("As unsigned integers:\n");
    printf("  QWords: ");
    for (int i = 0; i < 4; ++i) {
        printf("0x%016llX ", view.qwords[i]);
    }
    printf("\n");
    
    printf("  DWords: ");
    for (int i = 0; i < 8; ++i) {
        printf("0x%08X ", view.dwords[i]);
    }
    printf("\n");
    
    printf("  Words: ");
    for (int i = 0; i < 16; ++i) {
        printf("0x%04X ", view.words[i]);
        if ((i + 1) % 8 == 0) printf("\n         ");
    }
    printf("\n");
}
```

### Vector Operations
```cpp
// Basic vector operations
M256BIT AddM256BIT(M256BIT const& a, M256BIT const& b)
{
    M256BIT result;
    result.Low = AddM128BIT(a.Low, b.Low);
    result.High = AddM128BIT(a.High, b.High);
    return result;
}

M256BIT SubtractM256BIT(M256BIT const& a, M256BIT const& b)
{
    M256BIT result;
    result.Low = SubtractM128BIT(a.Low, b.Low);
    result.High = SubtractM128BIT(a.High, b.High);
    return result;
}

bool CompareM256BIT(M256BIT const& a, M256BIT const& b)
{
    return CompareM128BIT(a.Low, b.Low) && CompareM128BIT(a.High, b.High);
}

M256BIT BitwiseAND(M256BIT const& a, M256BIT const& b)
{
    M256BIT result;
    result.Low = BitwiseAND(a.Low, b.Low);
    result.High = BitwiseAND(a.High, b.High);
    return result;
}

M256BIT BitwiseOR(M256BIT const& a, M256BIT const& b)
{
    M256BIT result;
    result.Low = BitwiseOR(a.Low, b.Low);
    result.High = BitwiseOR(a.High, b.High);
    return result;
}

M256BIT BitwiseXOR(M256BIT const& a, M256BIT const& b)
{
    M256BIT result;
    result.Low = BitwiseXOR(a.Low, b.Low);
    result.High = BitwiseXOR(a.High, b.High);
    return result;
}

M256BIT BitwiseNOT(M256BIT const& a)
{
    M256BIT result;
    result.Low = BitwiseXOR(a.Low, CreateAllOnesM128BIT());
    result.High = BitwiseXOR(a.High, CreateAllOnesM128BIT());
    return result;
}
```

### AVX Register Context Usage
```cpp
// Extract from YMM register (AVX)
M256BIT ExtractFromYMMRegister(void const* ymmRegPtr)
{
    M256BIT result;
    memcpy(&result, ymmRegPtr, sizeof(M256BIT));
    return result;
}

// Set YMM register value
void SetYMMRegister(void* ymmRegPtr, M256BIT const& value)
{
    memcpy(ymmRegPtr, &value, sizeof(M256BIT));
}

// Process YMM registers from extended context
void ProcessYMMRegisters(AVX_YMM_REGISTERS const& ymmRegs)
{
    printf("YMM Registers:\n");
    
    // Access individual YMM registers through the structure
    M256BIT const* ymmArray = reinterpret_cast<M256BIT const*>(&ymmRegs);
    
    for (int i = 0; i < 16; ++i) {  // Assuming 16 YMM registers
        printf("YMM%d:\n", i);
        DisplayM256BIT(ymmArray[i]);
    }
}
```

### Lane-Based Operations
```cpp
// AVX operations work on 128-bit lanes
struct LaneOperations
{
    // Extract 128-bit lanes
    static M128BIT ExtractLowLane(M256BIT const& value)
    {
        return value.Low;
    }
    
    static M128BIT ExtractHighLane(M256BIT const& value)
    {
        return value.High;
    }
    
    // Insert 128-bit lane
    static M256BIT InsertLowLane(M256BIT const& value, M128BIT const& lane)
    {
        M256BIT result = value;
        result.Low = lane;
        return result;
    }
    
    static M256BIT InsertHighLane(M256BIT const& value, M128BIT const& lane)
    {
        M256BIT result = value;
        result.High = lane;
        return result;
    }
    
    // Permute lanes
    static M256BIT PermuteLanes(M256BIT const& value, bool swapLanes)
    {
        if (swapLanes) {
            M256BIT result;
            result.Low = value.High;
            result.High = value.Low;
            return result;
        }
        return value;
    }
    
    // Broadcast 128-bit lane
    static M256BIT BroadcastLane(M128BIT const& lane)
    {
        M256BIT result;
        result.Low = lane;
        result.High = lane;
        return result;
    }
};
```

### Conversion and Utility Functions
```cpp
// Convert to string representation
std::string M256BitToHexString(M256BIT const& value)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::uppercase;
    
    // High 128 bits first
    oss << std::setw(16) << static_cast<uint64_t>(value.High.High);
    oss << std::setw(16) << value.High.Low;
    // Low 128 bits
    oss << std::setw(16) << static_cast<uint64_t>(value.Low.High);
    oss << std::setw(16) << value.Low.Low;
    
    return oss.str();
}

// Parse from string
bool ParseM256BitFromHex(std::string const& hexStr, M256BIT& result)
{
    if (hexStr.length() != 64) {
        return false;
    }
    
    try {
        // Parse 128-bit components
        std::string highHighStr = hexStr.substr(0, 16);
        std::string highLowStr = hexStr.substr(16, 16);
        std::string lowHighStr = hexStr.substr(32, 16);
        std::string lowLowStr = hexStr.substr(48, 16);
        
        result.High.High = static_cast<int64_t>(std::stoull(highHighStr, nullptr, 16));
        result.High.Low = std::stoull(highLowStr, nullptr, 16);
        result.Low.High = static_cast<int64_t>(std::stoull(lowHighStr, nullptr, 16));
        result.Low.Low = std::stoull(lowLowStr, nullptr, 16);
        
        return true;
    } catch (...) {
        return false;
    }
}

// Zero initialization
M256BIT ZeroM256BIT()
{
    M256BIT result;
    result.Low = ZeroM128BIT();
    result.High = ZeroM128BIT();
    return result;
}

// Check if zero
bool IsZeroM256BIT(M256BIT const& value)
{
    return IsZeroM128BIT(value.Low) && IsZeroM128BIT(value.High);
}

// Create all ones
M256BIT CreateAllOnesM256BIT()
{
    M256BIT result;
    result.Low = CreateAllOnesM128BIT();
    result.High = CreateAllOnesM128BIT();
    return result;
}

// Create from bytes
M256BIT CreateM256BitFromBytes(uint8_t const bytes[32])
{
    M256BIT result;
    memcpy(&result, bytes, 32);
    return result;
}

// Extract to bytes
void ExtractM256BitToBytes(M256BIT const& value, uint8_t bytes[32])
{
    memcpy(bytes, &value, 32);
}
```

### Shift and Rotate Operations
```cpp
// 256-bit shift operations
M256BIT ShiftLeftM256BIT(M256BIT const& value, int bits)
{
    if (bits >= 256) {
        return ZeroM256BIT();
    }
    
    M256BIT result = value;
    
    if (bits >= 128) {
        // Shift more than 128 bits
        result.High = ShiftLeft(result.Low, bits - 128);
        result.Low = ZeroM128BIT();
    } else if (bits > 0) {
        // Shift within 256 bits
        M128BIT carry = ShiftRight(result.Low, 128 - bits);
        result.Low = ShiftLeft(result.Low, bits);
        
        M128BIT shiftedHigh = ShiftLeft(result.High, bits);
        result.High = BitwiseOR(shiftedHigh, carry);
    }
    
    return result;
}

M256BIT ShiftRightM256BIT(M256BIT const& value, int bits)
{
    if (bits >= 256) {
        return ZeroM256BIT();
    }
    
    M256BIT result = value;
    
    if (bits >= 128) {
        // Shift more than 128 bits
        result.Low = ShiftRight(result.High, bits - 128);
        result.High = ZeroM128BIT();
    } else if (bits > 0) {
        // Shift within 256 bits
        M128BIT carry = ShiftLeft(result.High, 128 - bits);
        result.High = ShiftRight(result.High, bits);
        
        M128BIT shiftedLow = ShiftRight(result.Low, bits);
        result.Low = BitwiseOR(shiftedLow, carry);
    }
    
    return result;
}
```

### Advanced Analysis Tools
```cpp
// Analyze 256-bit register content
struct M256BitAnalysis
{
    bool isAllZeros;
    bool isAllOnes;
    bool hasValidFloats;
    bool hasValidDoubles;
    bool isLaneSymmetric;  // Same data in both 128-bit lanes
    size_t nonZeroBytes;
    double entropyScore;
    std::array<bool, 2> laneActivity; // Activity in each 128-bit lane
};

M256BitAnalysis AnalyzeM256Bit(M256BIT const& value)
{
    M256BitAnalysis analysis{};
    
    // Basic checks
    analysis.isAllZeros = IsZeroM256BIT(value);
    analysis.isAllOnes = CompareM256BIT(value, CreateAllOnesM256BIT());
    analysis.isLaneSymmetric = CompareM128BIT(value.Low, value.High);
    
    // Lane activity
    analysis.laneActivity[0] = !IsZeroM128BIT(value.Low);
    analysis.laneActivity[1] = !IsZeroM128BIT(value.High);
    
    // Count non-zero bytes
    uint8_t const* bytes = reinterpret_cast<uint8_t const*>(&value);
    analysis.nonZeroBytes = 0;
    for (int i = 0; i < 32; ++i) {
        if (bytes[i] != 0) {
            analysis.nonZeroBytes++;
        }
    }
    
    // Validate floating-point data
    float const* floats = reinterpret_cast<float const*>(&value);
    analysis.hasValidFloats = true;
    for (int i = 0; i < 8; ++i) {
        if (!std::isfinite(floats[i])) {
            analysis.hasValidFloats = false;
            break;
        }
    }
    
    double const* doubles = reinterpret_cast<double const*>(&value);
    analysis.hasValidDoubles = true;
    for (int i = 0; i < 4; ++i) {
        if (!std::isfinite(doubles[i])) {
            analysis.hasValidDoubles = false;
            break;
        }
    }
    
    // Entropy calculation
    std::map<uint8_t, int> byteFreq;
    for (int i = 0; i < 32; ++i) {
        byteFreq[bytes[i]]++;
    }
    
    analysis.entropyScore = 0.0;
    for (auto const& pair : byteFreq) {
        double prob = static_cast<double>(pair.second) / 32.0;
        if (prob > 0) {
            analysis.entropyScore -= prob * log2(prob);
        }
    }
    
    return analysis;
}

void DisplayM256BitAnalysis(M256BIT const& value)
{
    auto analysis = AnalyzeM256Bit(value);
    
    printf("M256BIT Analysis:\n");
    printf("  All zeros: %s\n", analysis.isAllZeros ? "Yes" : "No");
    printf("  All ones: %s\n", analysis.isAllOnes ? "Yes" : "No");
    printf("  Lane symmetric: %s\n", analysis.isLaneSymmetric ? "Yes" : "No");
    printf("  Non-zero bytes: %zu/32\n", analysis.nonZeroBytes);
    printf("  Entropy: %.2f bits\n", analysis.entropyScore);
    printf("  Valid floats: %s\n", analysis.hasValidFloats ? "Yes" : "No");
    printf("  Valid doubles: %s\n", analysis.hasValidDoubles ? "Yes" : "No");
    printf("  Lane activity: Low=%s, High=%s\n",
           analysis.laneActivity[0] ? "Active" : "Inactive",
           analysis.laneActivity[1] ? "Active" : "Inactive");
}
```

### Memory Layout and Alignment
```cpp
// Ensure proper alignment for AVX operations
class AlignedM256BIT
{
    alignas(32) M256BIT data_;
    
public:
    AlignedM256BIT() : data_(ZeroM256BIT()) {}
    AlignedM256BIT(M256BIT const& value) : data_(value) {}
    
    M256BIT& Get() { return data_; }
    M256BIT const& Get() const { return data_; }
    
    void* GetAlignedPtr() { return &data_; }
    void const* GetAlignedPtr() const { return &data_; }
    
    bool IsAligned() const 
    {
        return (reinterpret_cast<uintptr_t>(&data_) % 32) == 0;
    }
};

// Memory operations
void SafeCopyM256BIT(M256BIT& dest, M256BIT const& src)
{
    // Use aligned copy if possible
    if (IsAligned(&dest) && IsAligned(&src)) {
        // Could use vectorized copy here
        memcpy(&dest, &src, sizeof(M256BIT));
    } else {
        // Fallback to safe copy
        dest.Low = src.Low;
        dest.High = src.High;
    }
}
```

## Important Notes

- M256BIT is composed of two M128BIT structures for compatibility
- AVX instructions operate on 256-bit data with 128-bit lane restrictions
- Many AVX operations preserve lane boundaries (no cross-lane operations)
- Proper memory alignment (32-byte) is crucial for optimal performance
- Lane-based architecture affects how data is processed in parallel
- Register values represent snapshots from specific execution points
- Mixed precision operations require careful data interpretation

## See Also

- [`M128BIT`](struct-M128BIT.md) - Base 128-bit SIMD data type used in M256BIT
- [`M512BIT`](struct-M512BIT.md) - Extended 512-bit SIMD data type
- [`AVX_YMM_REGISTERS`](struct-AVX_YMM_REGISTERS.md) - Array structure containing M256BIT registers
- [`VECTOR_256BIT_REGISTERS`](struct-VECTOR_256BIT_REGISTERS.md) - ZMM register 256-bit components
