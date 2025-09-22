// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License. See LICENSE in the project root for license information.

// RegisterNameMapping.h - Methods to map register name to location within CROSS_PLATFORM_CONTEXT structure.

#pragma once

#include <TTD/IReplayEngine.h>
#include <TTD/IReplayEngineRegisters.h>

#include <algorithm>
#include <array>
#include <ranges>
#include <span>
#include <stdint.h>
#include <string>
#include <utility>

namespace TTD
{

// Additional information about a register
enum class RegisterNameFlags
{
    None  = 0x0,
    Alias = 0x1,    // Register is an alias for another register
};
TTD_DEFINE_ENUM_FLAG_OPERATORS(RegisterNameFlags)

// Information about a register in the CROSS_PLATFORM_CONTEXT structure.
struct ContextPosition
{
    uint64_t    Offset{ 0 };
    uint64_t    Size{ 0 };
    RegisterNameFlags Flags{ RegisterNameFlags::None };

    auto operator<=>(ContextPosition const&) const noexcept = default;
    constexpr bool operator==(ContextPosition const& other) const noexcept = default;
};

// Returns true if the register is an alias for another register (e.g. al is an alias for rax in x64).
constexpr bool IsAliasedRegister(ContextPosition const& position)
{
    return (position.Flags & RegisterNameFlags::Alias) == RegisterNameFlags::Alias;
}

// Creates a register entry that spans the entire width of a register.
// FUTURE: Consider redoing using constexpr functions. Make a type alias
//         for the row of the register table. This will eliminate casting
//         the name to std::wstring_view, and you can put proper types
//         in some of these autos.
#define REG_TO_CONTEXT_SPAN(name, field) std::pair { \
    std::wstring_view{name}, \
    ContextPosition { \
        offsetof(Context, field), \
        sizeof(Context::field), \
    }}

// Creates a register entry that spans a specific number of bytes in the
// low portion of a register.
#define REG_TO_CONTEXT_SPAN_SIZE(name, field, size) std::pair { \
    std::wstring_view{name}, \
    ContextPosition { \
        offsetof(Context, field), \
        size, \
        RegisterNameFlags::Alias \
    }}

// Creates a register entry that spans a specific number of bytes in a
// portion of a register, starting at the specified byte offset.
#define REG_TO_CONTEXT_SPAN_OFFSET(name, field, size, offset) std::pair { \
    std::wstring_view{name}, \
    ContextPosition { \
        offsetof(Context, field) + offset, \
        size, \
        RegisterNameFlags::Alias \
    }}

// Table mapping x86 register names to ContextPositions, sorted
// lexicographically by register name.
constexpr auto c_x86RegisterNameToContextSpan = []() consteval
{
    using Context = X86_NT5_CONTEXT;

    std::array result =
    {
        REG_TO_CONTEXT_SPAN_SIZE( L"al", Eax, 1),
        REG_TO_CONTEXT_SPAN_OFFSET( L"ah", Eax, 1, 1),
        REG_TO_CONTEXT_SPAN_SIZE( L"ax", Eax, 2),
        REG_TO_CONTEXT_SPAN     (L"eax", Eax),

        REG_TO_CONTEXT_SPAN_SIZE(L" bl", Ebx, 1),
        REG_TO_CONTEXT_SPAN_OFFSET(L" bh", Ebx, 1, 1),
        REG_TO_CONTEXT_SPAN_SIZE(L" bx", Ebx, 2),
        REG_TO_CONTEXT_SPAN     (L"ebx", Ebx),

        REG_TO_CONTEXT_SPAN_SIZE( L"cl", Ecx, 1),
        REG_TO_CONTEXT_SPAN_OFFSET( L"ch", Ecx, 1, 1),
        REG_TO_CONTEXT_SPAN_SIZE( L"cx", Ecx, 2),
        REG_TO_CONTEXT_SPAN     (L"ecx", Ecx),

        REG_TO_CONTEXT_SPAN_SIZE( L"dl", Edx, 1),
        REG_TO_CONTEXT_SPAN_OFFSET( L"dh", Edx, 1, 1),
        REG_TO_CONTEXT_SPAN_SIZE( L"dx", Edx, 2),
        REG_TO_CONTEXT_SPAN     (L"edx", Edx),

        REG_TO_CONTEXT_SPAN_SIZE(L"sil", Esi, 1),
        REG_TO_CONTEXT_SPAN_SIZE( L"si", Esi, 2),
        REG_TO_CONTEXT_SPAN     (L"esi", Esi),

        REG_TO_CONTEXT_SPAN_SIZE(L"dil", Edi, 1),
        REG_TO_CONTEXT_SPAN_SIZE( L"di", Edi, 2),
        REG_TO_CONTEXT_SPAN     (L"edi", Edi),

        REG_TO_CONTEXT_SPAN_SIZE(L"bpl", Ebp, 1),
        REG_TO_CONTEXT_SPAN_SIZE( L"bp", Ebp, 2),
        REG_TO_CONTEXT_SPAN     (L"ebp", Ebp),

        REG_TO_CONTEXT_SPAN_SIZE(L"spl", Esp, 1),
        REG_TO_CONTEXT_SPAN_SIZE( L"sp", Esp, 2),
        REG_TO_CONTEXT_SPAN     (L"esp", Esp),

        REG_TO_CONTEXT_SPAN     (L"eip", Eip),

        // More esoteric but potentially useful registers
        REG_TO_CONTEXT_SPAN(L"contextflags", ContextFlags),
    };

    std::ranges::sort(result, [](auto const& lhs, auto const& rhs)
    {
        return lhs.first < rhs.first;
    });

    return result;
}();

// Table mapping x64 register names to ContextPositions, sorted
// lexicographically by register name.
constexpr auto c_x64RegisterNameToContextSpan = []() consteval
{
    using Context = AMD64_CONTEXT;

    std::array result =
    {
        REG_TO_CONTEXT_SPAN_SIZE(   L"al",   Rax, 1),
        REG_TO_CONTEXT_SPAN_OFFSET( L"ah",   Rax, 1, 1),
        REG_TO_CONTEXT_SPAN_SIZE(   L"ax",   Rax, 2),
        REG_TO_CONTEXT_SPAN_SIZE(  L"eax",   Rax, 4),
        REG_TO_CONTEXT_SPAN     (  L"rax",   Rax),

        REG_TO_CONTEXT_SPAN_SIZE(   L"bl",   Rbx, 1),
        REG_TO_CONTEXT_SPAN_OFFSET( L"bh",   Rbx, 1, 1),
        REG_TO_CONTEXT_SPAN_SIZE(   L"bx",   Rbx, 2),
        REG_TO_CONTEXT_SPAN_SIZE(  L"ebx",   Rbx, 4),
        REG_TO_CONTEXT_SPAN     (  L"rbx",   Rbx),

        REG_TO_CONTEXT_SPAN_SIZE(   L"cl",   Rcx, 1),
        REG_TO_CONTEXT_SPAN_OFFSET( L"ch",   Rcx, 1, 1),
        REG_TO_CONTEXT_SPAN_SIZE(   L"cx",   Rcx, 2),
        REG_TO_CONTEXT_SPAN_SIZE(  L"ecx",   Rcx, 4),
        REG_TO_CONTEXT_SPAN     (  L"rcx",   Rcx),

        REG_TO_CONTEXT_SPAN_SIZE(   L"dl",   Rdx, 1),
        REG_TO_CONTEXT_SPAN_OFFSET( L"dh",   Rdx, 1, 1),
        REG_TO_CONTEXT_SPAN_SIZE(   L"dx",   Rdx, 2),
        REG_TO_CONTEXT_SPAN_SIZE(  L"edx",   Rdx, 4),
        REG_TO_CONTEXT_SPAN     (  L"rdx",   Rdx),

        REG_TO_CONTEXT_SPAN_SIZE(  L"sil",   Rsi, 1),
        REG_TO_CONTEXT_SPAN_SIZE(   L"si",   Rsi, 2),
        REG_TO_CONTEXT_SPAN_SIZE(  L"esi",   Rsi, 4),
        REG_TO_CONTEXT_SPAN     (  L"rsi",   Rsi),

        REG_TO_CONTEXT_SPAN_SIZE(  L"dil",   Rdi, 1),
        REG_TO_CONTEXT_SPAN_SIZE(   L"di",   Rdi, 2),
        REG_TO_CONTEXT_SPAN_SIZE(  L"edi",   Rdi, 4),
        REG_TO_CONTEXT_SPAN     (  L"rdi",   Rdi),

        REG_TO_CONTEXT_SPAN_SIZE(  L"bpl",   Rbp, 1),
        REG_TO_CONTEXT_SPAN_SIZE(   L"bp",   Rbp, 2),
        REG_TO_CONTEXT_SPAN_SIZE(  L"ebp",   Rbp, 4),
        REG_TO_CONTEXT_SPAN     (  L"rbp",   Rbp),

        REG_TO_CONTEXT_SPAN_SIZE(  L"spl",   Rsp, 1),
        REG_TO_CONTEXT_SPAN_SIZE(   L"sp",   Rsp, 2),
        REG_TO_CONTEXT_SPAN_SIZE(  L"esp",   Rsp, 4),
        REG_TO_CONTEXT_SPAN     (  L"rsp",   Rsp),

        REG_TO_CONTEXT_SPAN_SIZE(  L"r8b",    R8, 1),
        REG_TO_CONTEXT_SPAN_SIZE(  L"r8w",    R8, 2),
        REG_TO_CONTEXT_SPAN_SIZE(  L"r8d",    R8, 4),
        REG_TO_CONTEXT_SPAN     (   L"r8",    R8),
        REG_TO_CONTEXT_SPAN_SIZE(  L"r9b",    R9, 1),
        REG_TO_CONTEXT_SPAN_SIZE(  L"r9w",    R9, 2),
        REG_TO_CONTEXT_SPAN_SIZE(  L"r9d",    R9, 4),
        REG_TO_CONTEXT_SPAN     (   L"r9",    R9),
        REG_TO_CONTEXT_SPAN_SIZE( L"r10b",   R10, 1),
        REG_TO_CONTEXT_SPAN_SIZE( L"r10w",   R10, 2),
        REG_TO_CONTEXT_SPAN_SIZE( L"r10d",   R10, 4),
        REG_TO_CONTEXT_SPAN     (  L"r10",   R10),
        REG_TO_CONTEXT_SPAN_SIZE( L"r11b",   R11, 1),
        REG_TO_CONTEXT_SPAN_SIZE( L"r11w",   R11, 2),
        REG_TO_CONTEXT_SPAN_SIZE( L"r11d",   R11, 4),
        REG_TO_CONTEXT_SPAN     (  L"r11",   R11),
        REG_TO_CONTEXT_SPAN_SIZE( L"r12b",   R12, 1),
        REG_TO_CONTEXT_SPAN_SIZE( L"r12w",   R12, 2),
        REG_TO_CONTEXT_SPAN_SIZE( L"r12d",   R12, 4),
        REG_TO_CONTEXT_SPAN     (  L"r12",   R12),
        REG_TO_CONTEXT_SPAN_SIZE( L"r13b",   R13, 1),
        REG_TO_CONTEXT_SPAN_SIZE( L"r13w",   R13, 2),
        REG_TO_CONTEXT_SPAN_SIZE( L"r13d",   R13, 4),
        REG_TO_CONTEXT_SPAN     (  L"r13",   R13),
        REG_TO_CONTEXT_SPAN_SIZE( L"r14b",   R14, 1),
        REG_TO_CONTEXT_SPAN_SIZE( L"r14w",   R14, 2),
        REG_TO_CONTEXT_SPAN_SIZE( L"r14d",   R14, 4),
        REG_TO_CONTEXT_SPAN     (  L"r14",   R14),
        REG_TO_CONTEXT_SPAN_SIZE( L"r15b",   R15, 1),
        REG_TO_CONTEXT_SPAN_SIZE( L"r15w",   R15, 2),
        REG_TO_CONTEXT_SPAN_SIZE( L"r15d",   R15, 4),
        REG_TO_CONTEXT_SPAN     (  L"r15",   R15),
        REG_TO_CONTEXT_SPAN     (  L"rip",   Rip),
        REG_TO_CONTEXT_SPAN     ( L"xmm0",  Xmm0),
        REG_TO_CONTEXT_SPAN     ( L"xmm1",  Xmm1),
        REG_TO_CONTEXT_SPAN     ( L"xmm2",  Xmm2),
        REG_TO_CONTEXT_SPAN     ( L"xmm3",  Xmm3),
        REG_TO_CONTEXT_SPAN     ( L"xmm4",  Xmm4),
        REG_TO_CONTEXT_SPAN     ( L"xmm5",  Xmm5),
        REG_TO_CONTEXT_SPAN     ( L"xmm6",  Xmm6),
        REG_TO_CONTEXT_SPAN     ( L"xmm7",  Xmm7),
        REG_TO_CONTEXT_SPAN     ( L"xmm8",  Xmm8),
        REG_TO_CONTEXT_SPAN     ( L"xmm9",  Xmm9),
        REG_TO_CONTEXT_SPAN     (L"xmm10", Xmm10),
        REG_TO_CONTEXT_SPAN     (L"xmm11", Xmm11),
        REG_TO_CONTEXT_SPAN     (L"xmm12", Xmm12),
        REG_TO_CONTEXT_SPAN     (L"xmm13", Xmm13),
        REG_TO_CONTEXT_SPAN     (L"xmm14", Xmm14),
        REG_TO_CONTEXT_SPAN     (L"xmm15", Xmm15),

        // TODO: Vector registers

        // More esoteric but potentially useful registers
        REG_TO_CONTEXT_SPAN(      L"eflags", EFlags),
        REG_TO_CONTEXT_SPAN(L"contextflags", ContextFlags),
        REG_TO_CONTEXT_SPAN(       L"mxcsr", MxCsr),
    };

    std::ranges::sort(result, [](auto const& lhs, auto const& rhs)
    {
        return lhs.first < rhs.first;
    });

    return result;
}();

// Table mapping ARM64 register names to ContextPositions, sorted
// lexicographically by register name.
constexpr auto c_Arm64RegisterNameToContextSpan = []() consteval
{
    using Context = ARM64_CONTEXT;

    std::array result =
    {
        REG_TO_CONTEXT_SPAN_SIZE( L"w0",  X[ 0], 4),
        REG_TO_CONTEXT_SPAN     ( L"x0",  X[ 0]),
        REG_TO_CONTEXT_SPAN_SIZE( L"w1",  X[ 1], 4),
        REG_TO_CONTEXT_SPAN     ( L"x1",  X[ 1]),
        REG_TO_CONTEXT_SPAN_SIZE( L"w2",  X[ 2], 4),
        REG_TO_CONTEXT_SPAN     ( L"x2",  X[ 2]),
        REG_TO_CONTEXT_SPAN_SIZE( L"w3",  X[ 3], 4),
        REG_TO_CONTEXT_SPAN     ( L"x3",  X[ 3]),
        REG_TO_CONTEXT_SPAN_SIZE( L"w4",  X[ 4], 4),
        REG_TO_CONTEXT_SPAN     ( L"x4",  X[ 4]),
        REG_TO_CONTEXT_SPAN_SIZE( L"w5",  X[ 5], 4),
        REG_TO_CONTEXT_SPAN     ( L"x5",  X[ 5]),
        REG_TO_CONTEXT_SPAN_SIZE( L"w6",  X[ 6], 4),
        REG_TO_CONTEXT_SPAN     ( L"x6",  X[ 6]),
        REG_TO_CONTEXT_SPAN_SIZE( L"w7",  X[ 7], 4),
        REG_TO_CONTEXT_SPAN     ( L"x7",  X[ 7]),
        REG_TO_CONTEXT_SPAN_SIZE( L"w8",  X[ 8], 4),
        REG_TO_CONTEXT_SPAN     ( L"x8",  X[ 8]),
        REG_TO_CONTEXT_SPAN_SIZE( L"w9",  X[ 9], 4),
        REG_TO_CONTEXT_SPAN     ( L"x9",  X[ 9]),
        REG_TO_CONTEXT_SPAN_SIZE(L"w10",  X[10], 4),
        REG_TO_CONTEXT_SPAN     (L"x10",  X[10]),
        REG_TO_CONTEXT_SPAN_SIZE(L"w11",  X[11], 4),
        REG_TO_CONTEXT_SPAN     (L"x11",  X[11]),
        REG_TO_CONTEXT_SPAN_SIZE(L"w12",  X[12], 4),
        REG_TO_CONTEXT_SPAN     (L"x12",  X[12]),
        REG_TO_CONTEXT_SPAN_SIZE(L"w13",  X[13], 4),
        REG_TO_CONTEXT_SPAN     (L"x13",  X[13]),
        REG_TO_CONTEXT_SPAN_SIZE(L"w14",  X[14], 4),
        REG_TO_CONTEXT_SPAN     (L"x14",  X[14]),
        REG_TO_CONTEXT_SPAN_SIZE(L"w15",  X[15], 4),
        REG_TO_CONTEXT_SPAN     (L"x15",  X[15]),
        REG_TO_CONTEXT_SPAN_SIZE(L"w16",  X[16], 4),
        REG_TO_CONTEXT_SPAN     (L"x16",  X[16]),
        REG_TO_CONTEXT_SPAN_SIZE(L"w17",  X[17], 4),
        REG_TO_CONTEXT_SPAN     (L"x17",  X[17]),
        REG_TO_CONTEXT_SPAN_SIZE(L"w18",  X[18], 4),
        REG_TO_CONTEXT_SPAN     (L"x18",  X[18]),
        REG_TO_CONTEXT_SPAN_SIZE(L"w19",  X[19], 4),
        REG_TO_CONTEXT_SPAN     (L"x19",  X[19]),
        REG_TO_CONTEXT_SPAN_SIZE(L"w20",  X[20], 4),
        REG_TO_CONTEXT_SPAN     (L"x20",  X[20]),
        REG_TO_CONTEXT_SPAN_SIZE(L"w21",  X[21], 4),
        REG_TO_CONTEXT_SPAN     (L"x21",  X[21]),
        REG_TO_CONTEXT_SPAN_SIZE(L"w22",  X[22], 4),
        REG_TO_CONTEXT_SPAN     (L"x22",  X[22]),
        REG_TO_CONTEXT_SPAN_SIZE(L"w23",  X[23], 4),
        REG_TO_CONTEXT_SPAN     (L"x23",  X[23]),
        REG_TO_CONTEXT_SPAN_SIZE(L"w24",  X[24], 4),
        REG_TO_CONTEXT_SPAN     (L"x24",  X[24]),
        REG_TO_CONTEXT_SPAN_SIZE(L"w25",  X[25], 4),
        REG_TO_CONTEXT_SPAN     (L"x25",  X[25]),
        REG_TO_CONTEXT_SPAN_SIZE(L"w26",  X[26], 4),
        REG_TO_CONTEXT_SPAN     (L"x26",  X[26]),
        REG_TO_CONTEXT_SPAN_SIZE(L"w27",  X[27], 4),
        REG_TO_CONTEXT_SPAN     (L"x27",  X[27]),
        REG_TO_CONTEXT_SPAN_SIZE(L"w28",  X[28], 4),
        REG_TO_CONTEXT_SPAN     (L"x28",  X[28]),
        REG_TO_CONTEXT_SPAN_SIZE(L"w29",  Fp, 4),
        REG_TO_CONTEXT_SPAN     (L"x29",  Fp),
        REG_TO_CONTEXT_SPAN_SIZE(L"w30",  Lr, 4),
        REG_TO_CONTEXT_SPAN     (L"x30",  Lr),
        REG_TO_CONTEXT_SPAN     ( L"fp",  Fp),
        REG_TO_CONTEXT_SPAN     ( L"lr",  Lr),
        REG_TO_CONTEXT_SPAN     ( L"sp",  Sp),
        REG_TO_CONTEXT_SPAN     ( L"pc",  Pc),

        // TODO: Vector registers

        // More esoteric but potentially useful registers
        REG_TO_CONTEXT_SPAN(L"contextflags", ContextFlags),
        REG_TO_CONTEXT_SPAN(        L"cpsr", Cpsr),
        REG_TO_CONTEXT_SPAN(        L"fpsr", Fpsr),
        REG_TO_CONTEXT_SPAN(        L"fpcr", Fpcr),
    };

    std::ranges::sort(result, [](auto const& lhs, auto const& rhs)
    {
        return lhs.first < rhs.first;
    });

    return result;
}();

using RegisterNameSpan = std::span<std::pair<std::wstring_view, ContextPosition> const>;

// Returns a span of register names and their corresponding ContextPositions for the given architecture.
constexpr RegisterNameSpan GetRegisterNameToContextMap(ProcessorArchitecture architecture)
{
    switch (architecture)
    {
        case   ProcessorArchitecture::x86: return { c_x86RegisterNameToContextSpan.data(),   c_x86RegisterNameToContextSpan.size()   };
        case   ProcessorArchitecture::x64: return { c_x64RegisterNameToContextSpan.data(),   c_x64RegisterNameToContextSpan.size()   };
        case ProcessorArchitecture::Arm64: return { c_Arm64RegisterNameToContextSpan.data(), c_Arm64RegisterNameToContextSpan.size() };
        default: return {};
    }
}

// Returns an iterator to the register name and ContextPosition for the given register name.
constexpr RegisterNameSpan::iterator GetRegisterContextPosition(
    ProcessorArchitecture architecture,
    std::wstring_view     registerName
    )
{
    auto const registerMap = GetRegisterNameToContextMap(architecture);
    auto const lower = std::ranges::lower_bound(registerMap, registerName, {}, &std::pair<std::wstring_view, ContextPosition>::first);
    if (lower == registerMap.end() || lower->first != registerName)
    {
        return registerMap.end();
    }
    return lower;
}

// Verify register name mapping works at compile time for each architecture
static_assert(GetRegisterContextPosition(ProcessorArchitecture::x86,   L"edx")->second == ContextPosition
{
    .Offset = offsetof(X86_NT5_CONTEXT, Edx),
    .Size   = 4,
});
static_assert(GetRegisterContextPosition(ProcessorArchitecture::x64,   L"rsp")->second == ContextPosition
{
    .Offset = offsetof(AMD64_CONTEXT, Rsp),
    .Size   = 8,
});
static_assert(GetRegisterContextPosition(ProcessorArchitecture::Arm64, L"w10")->second == ContextPosition
{
    .Offset = offsetof(ARM64_CONTEXT, X[10]),
    .Size   = 4,
    .Flags  = RegisterNameFlags::Alias
});

// Verify that aliased registers with non-zero offsets are correctly mapped
static_assert(GetRegisterContextPosition(ProcessorArchitecture::x86,   L"ch")->second == ContextPosition
{
    .Offset = offsetof(X86_NT5_CONTEXT, Ecx) + 1,
    .Size   = 1,
    .Flags  = RegisterNameFlags::Alias,
});

// Verify an unrecognized register name returns an end iterator
static_assert(GetRegisterContextPosition(ProcessorArchitecture::x86, L"unknown") == GetRegisterNameToContextMap(ProcessorArchitecture::x86).end());

} // namespace TTD
