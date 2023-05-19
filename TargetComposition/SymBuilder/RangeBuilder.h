//**************************************************************************
//
// RangeBuilder.h
//
// The header for the "range builder".  This is a set of objects which can take the parameters
// of a function and knowledge of its calling convention and walk the disassembly to determine
// the live ranges of each parameter.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#ifndef __RANGEBUILDER_H__
#define __RANGEBUILDER_H__

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
                                        _In_reads_(paramCount) VariableSymbol **ppParameters,
                                        _Out_writes_(paramCount) SvcSymbolLocation *pLocations) =0;

    // IsNonVolatile():
    //
    // Returns whether a register 'canonId' is non-volatile in the given calling convention or not.  Note that
    // the register id is given by the *CANONICAL* numbering of the register (often CodeView) and *NOT* the
    // domain specific register numbering that might be used by the disassembler.
    //
    virtual bool IsNonVolatile(_In_ ULONG canonId)
    {
        auto it = m_nonVolatiles.find(canonId);
        return (it != m_nonVolatiles.end());
    }

protected:

    SymbolBuilderManager *m_pManager;
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
                                        _In_reads_(paramCount) VariableSymbol **ppParameters,
                                        _Out_writes_(paramCount) SvcSymbolLocation *pLocations);

private:

    // Register identifiers for ordinal/floating point parameters (rcx/rdx/r8/r9, xmm0->3)
    std::vector<ULONG> m_ordIds;
    std::vector<ULONG> m_fltIds;

};

// CallingConvention_Windows_ARM64:
//
// Represents our understanding of the standard calling convention on Windows for ARM64.
//
class CallingConvention_Windows_ARM64 : public CallingConvention
{
public:
};

// RangeBuilder:
//
// The main range builder class.
//
class RangeBuilder
{
public:

    RangeBuilder();

    // PropagateParameterRanges():
    //
    // 
    void PropagateParameterRanges(_In_ FunctionSymbol *pFunction,
                                  _In_ CallingConvention *pConvention);

private:

    //*************************************************
    // Private Methods:
    //
    
    // TraverseBasicBlockAt():
    //
    // Walks through the instructions in the basic block starting at 'bbAddr' until it hits the end of the
    // basic block.  During the instruction traversal, propagate information we have about the state of parameters
    // at the start of the basic block through to the end of the basic block.
    //
    // If 'bbFrom' is not nullptr, the state from the end of 'bbFrom' will carry into the start of the 'bbAddr'
    // traversal.  If this does not change the live ranges at the start of 'bbAddr', the traversal is
    // considered complete.  Otherwise, the block will be traversed again to find and propagate any control flow 
    // dependent locations.
    //
    // If the block starting at 'bbAddr' is traversed, this will add outbound control flows from the traversed 
    // block to the traversal queue.
    //
    void TraverseBasicBlockAt(_In_ ULONG64 bbAddr, _In_ ULONG64 bbFrom = 0)

    // CarryoverLiveRanges():
    //
    // Takes the live ranges at the end of 'bbFrom' and carries them into 'bbTo'.  If this changes any live
    // ranges at the start of 'bbTo', this returns true; otherwise, this returns false.
    //
    bool CarryoverLiveRanges(_In_ BasicBlockInfo& bbFrom, _In_ BasicBlockInfo& bbTo);

    //*************************************************
    // Permanent State:
    //

    // The data model disassembler
    Object m_dis;

    //*************************************************
    // Ephemeral State:
    //

    //
    // Information about the function/module we are currently building parameter ranges for:
    //
    FunctionSymbol *m_pFunction;
    CallingConvention *m_pConvention;
    ULONG64 m_functionOffset;
    ULONG64 m_modBase;

    // BasicBlockInfo:
    //
    // Records information about a particular basic block.
    //
    struct BasicBlockInfo
    {
        //*************************************************
        // Data:
        //

        // The disassembler's representation of a basic block.
        Object BasicBlock;
        ULONG64 StartAddress;
        ULONG64 EndAddress;

        // How many times has our traversal entered this basic block.
        ULONG TraversalCount;

        //*************************************************
        // Constructors:
        //

        // Constructs a basic block info
        BasicBlockInfo(_In_ Object basicBlock) :
            BasicBlock(std::move(baasicBlock))
        {
            StartAddress = (ULONG64)BasicBlock.KeyValue(L"StartAddress");
            EndAddress = (ULONG64)BasicBlock.KeyValue(L"EndAddress");
            TraversalCount = 0;
        }
    };

    std::unordered_map<ULONG64, BasicBlockInfo> m_bbInfo;
    std::queue<std::pair<ULONG64, ULONG64>> m_bbTrav;
    std::vector<VariableSymbol *> m_parameters;


};

} // SymbolBuilder
} // Services
} // TargetComposition
} // Debugger
