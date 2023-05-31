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

    static constexpr ULONG NoRegister = static_cast<ULONG>(-1);

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
        size_t TraversalCountSlot;
    };

    enum class LiveState
    {
        // The given location is still live
        Live,
        
        // The given location is marked for kill during the processing of an instruction.  After the instruction
        // finishes processing, it will change to dead.
        MarkedForKill,

        // The given location is dead.  No further processing or aliasing is required.
        Dead,
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
        LiveState State;
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
        std::vector<ULONG> TraversalCountSlots;

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

    // RecognizedInstruction:
    //
    // Defines instructions that we recognize for specific purposes.
    //
    enum class RecognizedInstruction
    {
        Unknown,
        Mov,
        Push,
        Pop,
        Add,
        Sub,
        Lea
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

    enum OperandFlags
    {
        OperandInput = 0x00000001,
        OperandOutput = 0x000000002,
        OperandRegister = 0x00000004,
        OperandMemory = 0x00000008,
        OperandImmediate = 0x00000010,
    };

    // OperandInfo:
    //
    // Information about an operand
    //
    struct OperandInfo
    {
        ULONG Flags;                // OperandFlags set
        ULONG Regs[3];              // NoRegister if not present
        ULONG ScalingFactor;        // regs[0] * scalingFactor (1)
        LONG64 ConstantValue;       // immediate
    };

    std::unordered_map<ULONG64, BasicBlockInfo> m_bbInfo;
    std::queue<TraversalEntry> m_bbTrav;
    std::vector<VariableSymbol *> m_parameters;
    std::unordered_map<ULONG, ULONG> m_disRegToCanonical;           // Maps disassembler IDs to canonical ones

    //*************************************************
    // Private Methods:
    //

    // GetBaseRegister():
    //
    // Gets the base register of a given canonical register.  This will return the topmost parent for any
    // sub-register.  It will, for example, return 'rax' when passed ID for 'ah', 'al', 'ax', or 'eax'.
    //
    ULONG GetBaseRegister(_In_ ULONG canonId);

    // GetCanonicalRegisterIdFromObject():
    //
    // For a given register object from the data model disassembler, get its canonical ID that must be passed
    // upwards for our locations.
    //
    ULONG GetCanonicalRegisterId(_In_ Object regObj);

    // UsesRegister():
    //
    // Determines whether a given location uses a register.  This includes sub-register overlap.
    //
    bool UsesRegister(_In_ SvcSymbolLocation const& location, _In_ ULONG canonId);

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

    // CheckForKill():
    //
    // Checks whether a given operand as a destination will kill the live range given by lr.
    //
    bool CheckForKill(_In_ OperandInfo const& opInfo, _In_ LocationRange const &lr);

    // CheckAddAlias():
    //
    // For the instruction at 'instrAddr' where that instruction is functionally a mov of inputInfo to outputInfo, 
    // see if this creates an aliasing of 'lr'.  If so, add such aliasing to 'ranges' starting at 
    // 'instrAddr' + 'instrLen' and return true; otherwise, return false.
    //
    bool CheckAddAlias(_In_ ULONG64 instrAddr,
                       _In_ ULONG64 instrLen,
                       _In_ OperandInfo const& outputInfo,
                       _In_ OperandInfo const& intputInfo,
                       _In_ LocationRange const& lr,
                       _Inout_ ParameterRanges& ranges);

    // GetOperandInfo():
    //
    // Gets operand information from a data model disassembler operand object.
    //
    void GetOperandInfo(_In_ Object& operand, _Out_ OperandInfo *pOperandInfo);

    // OperandToLocation():
    //
    // Given the operand, fill in a location structure for it.  False is returned if we cannot do such.
    //
    bool OperandToLocation(_In_ OperandInfo const& opInfo, _Out_ SvcSymbolLocation *pLocation);

    // BuildOperand():
    //
    // Builds an OperandInfo for a given register/memory/immediate operand that uses a single register.
    //
    void BuildOperand(_In_ ULONG registerNumber,
                      _In_ bool output,
                      _Out_ OperandInfo& opInfo,
                      _In_ LONG64 immediate = 0,
                      _In_ bool memory = false,
                      _In_ ULONG scalingFactor = 1)
    {
        for (size_t r = 0; r < ARRAYSIZE(opInfo.Regs); ++r)
        {
            opInfo.Regs[r] = NoRegister;
        }
        opInfo.Flags = (output) ? OperandOutput : OperandInput;
        if (registerNumber != NoRegister)
        {
            opInfo.Regs[0] = registerNumber;
            opInfo.Flags |= memory ? OperandMemory : OperandRegister;
        }
        if (immediate != 0)
        {
            opInfo.Flags |= OperandImmediate;
        }
        opInfo.ScalingFactor = scalingFactor;
        opInfo.ConstantValue = immediate;
    }

    //*************************************************
    // Instruction Helpers:
    //

    // GetRecognizedInstruction():
    //
    // Is this a "*" instruction (or the equivalent on whatever architecture we understand)
    //
    RecognizedInstruction GetRecognizedInstruction(_In_ std::wstring const& mnemonic);

};

} // SymbolBuilder
} // Services
} // TargetComposition
} // Debugger

#endif // __RANGEBUILDER_H__
