# IReplayEngineRegisters.h

# REVIEW: Decide what needs to be emphasized from this header and update this text. Suggest documenting `CROSS_PLATFORM_CONTEXT` and `AVX_EXTENDED_CONTEXT` at a minimum, and referring to the header for per-architecture details.

Cross-platform processor register context structures and SIMD register definitions for Time Travel Debugging (TTD) replay engine. This header provides comprehensive register state representation for x86, x64, ARM, and ARM64 architectures, including advanced SIMD extensions.

## Overview

IReplayEngineRegisters.h defines register context structures that capture the complete processor state during TTD recordings. It provides cross-platform compatibility by supporting multiple processor architectures and their respective register sets, including general-purpose registers, floating-point units, vector registers, and debug registers.

## Architecture Support

### x86 (32-bit Intel/AMD)
- General-purpose registers (EAX, EBX, ECX, EDX, ESI, EDI, EBP, ESP, EIP)
- Segment registers (CS, DS, ES, FS, GS, SS)
- Floating-point state (80387 and FXSAVE formats)
- Debug registers (DR0-DR7)
- Extended registers for SIMD operations

### x64 (64-bit AMD64/Intel64)
- Extended general-purpose registers (RAX-R15)
- Parameter home addresses (P1Home-P6Home)
- XMM registers (Xmm0-Xmm15)
- Vector registers with AVX/AVX2/AVX-512 support
- Debug control registers
- Branch tracing support

### ARM (32-bit)
- General-purpose registers (R0-R12, SP, LR, PC)
- NEON floating-point registers (Q, D, S formats)
- Breakpoint and watchpoint registers
- Program status register (CPSR)

### ARM64 (64-bit AArch64)
- Extended general-purpose registers (X0-X28, FP, LR, SP, PC)
- Advanced SIMD registers (V0-V31)
- Enhanced debug capabilities
- Floating-point control registers

## Key Components

### SIMD Data Types
- [`M128BIT`](struct-M128BIT.md) - 128-bit SIMD data type
- [`M256BIT`](struct-M256BIT.md) - 256-bit SIMD data type  
- [`M512BIT`](struct-M512BIT.md) - 512-bit SIMD data type

### x86 Architecture
- [`X86_FXSAVE_FORMAT`](struct-X86_FXSAVE_FORMAT.md) - x86 FXSAVE register format
- [`X86_FLOATING_SAVE_AREA`](struct-X86_FLOATING_SAVE_AREA.md) - x86 80387 floating-point save area
- [`X86_NT5_CONTEXT`](struct-X86_NT5_CONTEXT.md) - Complete x86 processor context

### x64 Architecture  
- [`AMD64_XMM_SAVE_AREA32`](struct-AMD64_XMM_SAVE_AREA32.md) - AMD64 XMM register save area
- [`AMD64_CONTEXT`](struct-AMD64_CONTEXT.md) - Complete AMD64 processor context

### ARM Architecture
- [`ARM_CONTEXT`](struct-ARM_CONTEXT.md) - Complete ARM processor context

### ARM64 Architecture
- [`ARM64_NEON128`](union-ARM64_NEON128.md) - ARM64 NEON 128-bit register union
- [`ARM64_CONTEXT`](struct-ARM64_CONTEXT.md) - Complete ARM64 processor context

### Vector Register Structures
- [`VECTOR_128BIT_REGISTERS`](struct-VECTOR_128BIT_REGISTERS.md) - 128-bit vector register array
- [`VECTOR_256BIT_REGISTERS`](struct-VECTOR_256BIT_REGISTERS.md) - 256-bit vector register array
- [`VECTOR_512BIT_REGISTERS`](struct-VECTOR_512BIT_REGISTERS.md) - 512-bit vector register array
- [`AVX_YMM_REGISTERS`](struct-AVX_YMM_REGISTERS.md) - AVX YMM register structure
- [`AVX_ZMM_REGISTERS`](struct-AVX_ZMM_REGISTERS.md) - AVX-512 ZMM register structure
- [`OPMASK_REGISTERS`](struct-OPMASK_REGISTERS.md) - AVX-512 opmask registers
- [`AVX_512_K_REGISTERS`](struct-AVX_512_K_REGISTERS.md) - AVX-512 K register structure

### Cross-Platform Support
- [`CROSS_PLATFORM_CONTEXT`](struct-CROSS_PLATFORM_CONTEXT.md) - Universal register context union
- [`AVX_EXTENDED_CONTEXT`](struct-AVX_EXTENDED_CONTEXT.md) - Extended vector register context

## Register Context Flags

### x86 Context Flags
```cpp
#define VDMCONTEXT_CONTROL         (VDMCONTEXT_i386 | 0x00000001L) // SS:SP, CS:IP, FLAGS, BP
#define VDMCONTEXT_INTEGER         (VDMCONTEXT_i386 | 0x00000002L) // AX, BX, CX, DX, SI, DI
#define VDMCONTEXT_SEGMENTS        (VDMCONTEXT_i386 | 0x00000004L) // DS, ES, FS, GS
#define VDMCONTEXT_FLOATING_POINT  (VDMCONTEXT_i386 | 0x00000008L) // 387 state
#define VDMCONTEXT_DEBUG_REGISTERS (VDMCONTEXT_i386 | 0x00000010L) // DB 0-3,6,7
#define VDMCONTEXT_EXTENDED_REGISTERS  (VDMCONTEXT_i386 | 0x00000020L) // cpu specific extensions
```

### AMD64 Context Flags  
```cpp
#define AMD64_CONTEXT_CONTROL           (AMD64_CONTEXT_AMD64 | 0x00000001L)
#define AMD64_CONTEXT_INTEGER           (AMD64_CONTEXT_AMD64 | 0x00000002L)
#define AMD64_CONTEXT_SEGMENTS          (AMD64_CONTEXT_AMD64 | 0x00000004L)
#define AMD64_CONTEXT_FLOATING_POINT    (AMD64_CONTEXT_AMD64 | 0x00000008L)
#define AMD64_CONTEXT_DEBUG_REGISTERS   (AMD64_CONTEXT_AMD64 | 0x00000010L)
```

### ARM Context Flags
```cpp
#define ARM_CONTEXT_CONTROL             (ARM_CONTEXT_ARM | 0x00000001L)
#define ARM_CONTEXT_INTEGER             (ARM_CONTEXT_ARM | 0x00000002L)
#define ARM_CONTEXT_FLOATING_POINT      (ARM_CONTEXT_ARM | 0x00000004L)
#define ARM_CONTEXT_DEBUG_REGISTERS     (ARM_CONTEXT_ARM | 0x00000008L)
```

### ARM64 Context Flags
```cpp
#define ARM64_CONTEXT_CONTROL (ARM64_CONTEXT_ARM64 | 0x1L)
#define ARM64_CONTEXT_INTEGER (ARM64_CONTEXT_ARM64 | 0x2L)
#define ARM64_CONTEXT_FLOATING_POINT  (ARM64_CONTEXT_ARM64 | 0x4L)
#define ARM64_CONTEXT_DEBUG_REGISTERS (ARM64_CONTEXT_ARM64 | 0x8L)
```

## Usage Patterns

### Architecture Detection
```cpp
// Determine architecture from context flags
bool IsX86Context(uint32_t contextFlags)
{
    return (contextFlags & VDMCONTEXT_i386) != 0;
}

bool IsAMD64Context(uint32_t contextFlags)
{
    return (contextFlags & AMD64_CONTEXT_AMD64) != 0;
}

bool IsARMContext(uint32_t contextFlags)
{
    return (contextFlags & ARM_CONTEXT_ARM) != 0;
}

bool IsARM64Context(uint32_t contextFlags)
{
    return (contextFlags & ARM64_CONTEXT_ARM64) != 0;
}
```

### Register Access Patterns
```cpp
// Safe register access across architectures
uint64_t GetInstructionPointer(CROSS_PLATFORM_CONTEXT const& context)
{
    if (IsX86Context(context.X86Nt5Context.ContextFlags)) {
        return context.X86Nt5Context.Eip;
    } else if (IsAMD64Context(context.Amd64Context.ContextFlags)) {
        return context.Amd64Context.Rip;
    } else if (IsARMContext(context.ArmContext.ContextFlags)) {
        return context.ArmContext.Pc;
    } else if (IsARM64Context(context.Arm64Context.ContextFlags)) {
        return context.Arm64Context.Pc;
    }
    return 0;
}

uint64_t GetStackPointer(CROSS_PLATFORM_CONTEXT const& context)
{
    if (IsX86Context(context.X86Nt5Context.ContextFlags)) {
        return context.X86Nt5Context.Esp;
    } else if (IsAMD64Context(context.Amd64Context.ContextFlags)) {
        return context.Amd64Context.Rsp;
    } else if (IsARMContext(context.ArmContext.ContextFlags)) {
        return context.ArmContext.Sp;
    } else if (IsARM64Context(context.Arm64Context.ContextFlags)) {
        return context.Arm64Context.Sp;
    }
    return 0;
}
```

### SIMD Register Operations
```cpp
// Extract SIMD data across different formats
void ProcessSIMDData(M128BIT const& simdReg)
{
    // Access as different data types
    uint64_t low64 = simdReg.Low;
    int64_t high64 = simdReg.High;
    
    // Interpret as floating-point values
    double* doubleValues = reinterpret_cast<double const*>(&simdReg);
    float* floatValues = reinterpret_cast<float const*>(&simdReg);
    
    // Process vector data
    for (int i = 0; i < 2; ++i) {
        printf("Double[%d]: %f\n", i, doubleValues[i]);
    }
    for (int i = 0; i < 4; ++i) {
        printf("Float[%d]: %f\n", i, floatValues[i]);
    }
}
```

### Debug Register Management
```cpp
// Cross-platform debug register access
struct DebugRegisterState
{
    uint64_t breakpoints[8];
    uint64_t watchpoints[8];
    size_t breakpointCount;
    size_t watchpointCount;
};

DebugRegisterState ExtractDebugState(CROSS_PLATFORM_CONTEXT const& context)
{
    DebugRegisterState state{};
    
    if (IsAMD64Context(context.Amd64Context.ContextFlags)) {
        state.breakpoints[0] = context.Amd64Context.Dr0;
        state.breakpoints[1] = context.Amd64Context.Dr1;
        state.breakpoints[2] = context.Amd64Context.Dr2;
        state.breakpoints[3] = context.Amd64Context.Dr3;
        state.breakpointCount = 4;
    } else if (IsARM64Context(context.Arm64Context.ContextFlags)) {
        for (size_t i = 0; i < ARM64_MAX_BREAKPOINTS; ++i) {
            state.breakpoints[i] = context.Arm64Context.Bvr[i];
        }
        for (size_t i = 0; i < ARM64_MAX_WATCHPOINTS; ++i) {
            state.watchpoints[i] = context.Arm64Context.Wvr[i];
        }
        state.breakpointCount = ARM64_MAX_BREAKPOINTS;
        state.watchpointCount = ARM64_MAX_WATCHPOINTS;
    }
    
    return state;
}
```

## Advanced Vector Extensions

### AVX-512 Support
- Supports up to 32 ZMM registers (Zmm0-Zmm31) with 512-bit width
- Opmask registers (K0-K7) for conditional SIMD operations
- Extended register sets for high-performance computing scenarios

### Cross-Platform SIMD
- Unified SIMD data types work across x86, x64, ARM, and ARM64
- Consistent access patterns for vector operations
- Support for mixed-precision computations

## Integration with TTD

The register structures integrate directly with TTD's replay engine through:

```cpp
// TTD integration points
TTD::Replay::RegisterContext registerContext;
TTD::Replay::ExtendedRegisterContext extendedContext;

// Conversion operators available
CROSS_PLATFORM_CONTEXT crossPlatformCtx = registerContext;
AVX_EXTENDED_CONTEXT avxExtendedCtx = extendedContext;
```

## Memory Alignment

All register structures follow strict alignment requirements:
- X86_CONTEXT: 4-byte alignment
- AMD64_CONTEXT: 16-byte alignment  
- ARM_CONTEXT: 8-byte alignment
- ARM64_CONTEXT: 16-byte alignment
- CROSS_PLATFORM_CONTEXT: Maximum alignment of contained types

## Performance Considerations

- Register context structures are optimized for fast copying during replay
- Union-based designs minimize memory overhead
- SIMD types enable efficient vector operations
- Cross-platform contexts use padding to ensure consistent layout

## Thread Safety

Register contexts are typically thread-local and don't require synchronization. However, when sharing contexts between threads:
- Use proper locking mechanisms
- Consider using immutable copies for analysis
- Be aware of architecture-specific differences in register sets

## Important Notes

- Context flags determine which register sets are valid
- Not all architectures support all register types
- SIMD register interpretation depends on the executing instruction
- Debug registers have architecture-specific capabilities
- Extended contexts may not be available on all systems
- Register values are captured at specific execution points during replay

## See Also

- [`IReplayEngine`](../IReplayEngine.h/README.md) - Main replay engine interface that uses these register contexts
- [`Position`](../IReplayEngine.h/struct-Position.md) - Timeline positions where register states are captured
- [`ICursorView`](../IReplayEngine.h/interface-ICursorView.md) - Cursor interface for accessing register states during replay
