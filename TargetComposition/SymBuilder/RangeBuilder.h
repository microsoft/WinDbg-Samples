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

using namespace Debugger::DataModel::ClientEx;

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

    // LocationInfo:
    //
    // Information about the location of a variable at a particular instruction.
    //
    struct LocationInfo
    {
        SvcSymbolLocation ParamLocation;
        ULONG TraversalCount;
    };

    // LocationRange:
    //
    // Defines a location over a range of instructions defined by a half-open set [StartAddress, EndAddress)
    //
    struct LocationRange
    {
        ULONG64 StartAddress;
        ULONG64 EndAddress;
        LocationInfo ParamLocation;
    };

    // ParameterRanges:
    //
    // The locations of a variable within a particular range of instructions.
    //
    using ParameterRanges = std::vector<LocationRange>;

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

        //
        // The locations of parameters within this basic block.
        //
        std::vector<ParameterRanges> BlockParameterRanges;

        // How many times has our traversal entered this basic block.
        ULONG TraversalCount;

        //*************************************************
        // Constructors:
        //

        // Constructs a basic block info
        BasicBlockInfo(_In_ Object basicBlock) :
            BasicBlock(std::move(basicBlock))
        {
            StartAddress = (ULONG64)BasicBlock.KeyValue(L"StartAddress");
            EndAddress = (ULONG64)BasicBlock.KeyValue(L"EndAddress");
            TraversalCount = 0;
        }
    };

    // TraversalEntry
    //
    // Describes the need to traverse BlockAddress as entered from the basic block starting at SourceBlockAddress.
    // The instruction which transferred control between blocks (whether a branch or fall through) is
    // SourceBlockInstructionAddress.
    //
    // The initial block will have 0 for source block fields.
    //
    struct TraversalEntry
    {
        ULONG64 BlockAddress;
        ULONG64 SourceBlockAddress;
        ULONG64 SourceBlockInstructionAddress;
    };

    std::unordered_map<ULONG64, BasicBlockInfo> m_bbInfo;
    std::queue<TraversalEntry> m_bbTrav;
    std::vector<VariableSymbol *> m_parameters;

    //*************************************************
    // Private Methods:
    //

    // CreateLiveRangeSets():
    //
    // Transfers live range data from our build to the parameters.
    //
    void CreateLiveRangeSets();

    // InitializeParameterLocations():
    //
    // Creates the initial placement of parameters via calling convention on the first instruction in the given
    // basic block (which should be the entry block into the given function).
    //
    void InitializeParameterLocations(_In_ CallingConvention *pConvention, _In_ BasicBlockInfo &entryBlock);
    
    // TraverseBasicBlock():
    //
    // Walks through the instructions in the basic block starting at 'entry.BlockAddress' until it hits the end of the
    // basic block.  During the instruction traversal, propagate information we have about the state of parameters
    // at the start of the basic block through to the end of the basic block.
    //
    // If 'entry.SourceBlock*' is not 0, the state from the end of that block will carry into the start of the 
    // traversal.  If this does not change the live ranges at the start of 'entry.BlockAddress', the traversal is
    // considered complete.  Otherwise, the block will be traversed again to find and propagate any control flow 
    // dependent locations.
    //
    // If the block starting at 'entry.BlockAddress' is traversed, this will add outbound control flows from the 
    // traversed block to the traversal queue.
    //
    void TraverseBasicBlock(_In_ TraversalEntry const& entry);

    // UpdateRangesForInstruction():
    // 
    // Updates parameter live range information *WITHIN* this basic block given the disassembled instruction
    // "instr".
    //
    void UpdateRangesForInstruction(_In_ BasicBlockInfo& block, _In_ Object instr);

    // CarryoverLiveRange():
    //
    // Takes a given live range and carries it into bbTo.  This returns whether or not a change was made
    // to 'bbTo'
    //
    bool CarryoverLiveRange(_In_ BasicBlockInfo& bbTo,
                            _In_ size_t paramNum,
                            _In_ LocationRange const& liveRange);

    // CarryoverLiveRanges():
    //
    // Takes the live ranges at the end of 'bbFrom' and carries them into 'bbTo'.  If this changes any live
    // ranges at the start of 'bbTo', this returns true; otherwise, this returns false.
    //
    bool CarryoverLiveRanges(_In_ BasicBlockInfo& bbFrom, _In_ BasicBlockInfo& bbTo, _In_ TraversalEntry const& entry);

    // AddParameterRangeToFunction()
    //
    // For the [startAddress, endAddress) half-open range (given by absolute VAs), add the range to the function
    // we are building data for.
    //
    bool AddParameterRangeToFunction(_In_ size_t paramNum,
                                     _Inout_ ULONG64 &startAddress,
                                     _Inout_ ULONG64 &endAddress,
                                     _In_ SvcSymbolLocation const& location);

};

} // SymbolBuilder
} // Services
} // TargetComposition
} // Debugger

#endif // __RANGEBUILDER_H__
