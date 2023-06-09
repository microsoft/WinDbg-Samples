//**************************************************************************
//
// CallingConvention.h
//
// The header for our understanding of calling conventions.
//
// NOTE: This particular component is on the cusp between the target model side of the
//       extension and the data model side.  It throws exceptions and that takes a great
//       deal of care with its usage on the target model side.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#ifndef __CALLINGCONVENTION_H__
#define __CALLINGCONVENTION_H__

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace SymbolBuilder
{

// CallingConvention:
//
// An abstraction of a calling convention.
//
class CallingConvention
{
public:

    // CallingConvention():
    //
    // Initializes a calling convention object.
    //
    CallingConvention(_In_ SymbolBuilderManager *pManager,
                      _In_ size_t numNonVolatiles,
                      _In_reads_(numNonVolatiles) wchar_t const **ppNonVolatileNames);

    // GetParameterPlacements():
    //
    // Given a list of parameters to a function, fill in their locations with knowledge of the calling
    // convention.
    //
    virtual void GetParameterPlacements(_In_ size_t paramCount,
                                        _In_reads_(paramCount) VariableSymbol const **ppParameters,
                                        _Out_writes_(paramCount) SvcSymbolLocation *pLocations) =0;

    // IsNonVolatile():
    //
    // Returns whether a register 'canonId' is non-volatile in the given calling convention or not.  Note that
    // the register id is given by the *CANONICAL* numbering of the register (often CodeView) and *NOT* the
    // domain specific register numbering that might be used by the disassembler.
    //
    // Note that we *MUST* take into account sub-registering.
    //
    virtual bool IsNonVolatile(_In_ ULONG canonId);

    // GetSpId():
    //
    // Get the register number of the stack pointer for the underlying architecture.
    //
    virtual ULONG GetSpId() =0;

protected:

    // FillRegisterCanonicalIds():
    //
    // Fills in the canonical IDs of registers given their name.
    //
    void FillRegisterCanonicalIds(_In_ size_t numRegisters,
                                  _In_reads_(numRegisters) wchar_t const **ppRegNames,
                                  _Out_writes_(numRegisters) ULONG *pRegIds);

    void FillRegisterCanonicalIds(_In_ size_t numRegisters,
                                  _In_reads_(numRegisters) wchar_t const **ppRegNames,
                                  _Inout_ std::vector<ULONG> &regIds)
    {
        regIds.resize(numRegisters);
        FillRegisterCanonicalIds(numRegisters, ppRegNames, &(regIds[0]));
    }

    SymbolBuilderManager *m_pManager;                   // It owns our lifetime!
    std::unordered_set<ULONG> m_nonVolatiles;
};

// CallingConvention_Windows_AMD64:
//
// Represents our understanding of the standard calling convention on Windows for AMD64.
//
class CallingConvention_Windows_AMD64 : public CallingConvention
{
public:

    CallingConvention_Windows_AMD64(_In_ SymbolBuilderManager *pManager);

    //
    // Given a list of parameters to a function, fill in their locations with knowledge of the calling
    // convention.
    //
    virtual void GetParameterPlacements(_In_ size_t paramCount,
                                        _In_reads_(paramCount) VariableSymbol const **ppParameters,
                                        _Out_writes_(paramCount) SvcSymbolLocation *pLocations);

    // GetSpId():
    //
    // Get the register number of the stack pointer for the underlying architecture.
    //
    virtual ULONG GetSpId() { return m_spId; }

private:

    // Register identifiers for ordinal/floating point parameters (rcx/rdx/r8/r9, xmm0->3)
    std::vector<ULONG> m_ordIds;
    std::vector<ULONG> m_fltIds;
    ULONG m_spId;

};

} // SymbolBuilder
} // Services
} // TargetComposition
} // Debugger

#endif // __CALLINGCONVENTION_H__

