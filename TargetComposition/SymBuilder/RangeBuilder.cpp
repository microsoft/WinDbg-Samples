//**************************************************************************
//
// RangeBuilder.cpp
//
// The implementation for the "range builder".  This is a set of objects which can take the parameters
// of a function and knowledge of its calling convention and walk the disassembly to determine
// the live ranges of each parameter.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#include "SymBuilder.h"
#include "ObjectModel.h"

using namespace Microsoft::WRL;

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace SymbolBuilder
{

//*************************************************
// General Range Builder:
//

bool LocationsAreEquivalent(_In_ const SvcSymbolLocation &a, _In_ const SvcSymbolLocation &b)
{
    return (a.Kind == b.Kind &&
            a.RegInfo.Number == b.RegInfo.Number &&
            a.RegInfo.Size == b.RegInfo.Size &&
            a.Offset == b.Offset);
}

RangeBuilder::RangeBuilder()
{
    //
    // Go and create an instance of the disassembler we can use for our walk of the disassembly.  This
    // will automatically trigger a loading of the data model disassembler and throw if that does not
    // succeed.
    //
    Object codeNS = Object::RootNamespace().KeyValue(L"Debugger").KeyValue(L"Utility").KeyValue(L"Code");
    m_dis = codeNS.CallMethod(L"CreateDisassembler");
}

ULONG RangeBuilder::GetBaseRegister(_In_ ULONG canonId)
{
    auto pSymManager = m_pFunction->InternalGetSymbolSet()->GetSymbolBuilderManager();
    for(;;)
    {
        RegisterInformation *pRegInfo;
        CheckHr(pSymManager->FindInformationForRegisterById(canonId, &pRegInfo));
        
        if (pRegInfo->ParentId == static_cast<ULONG>(-1))
        {
            return canonId;
        }
        canonId = pRegInfo->ParentId;
    }
}

ULONG RangeBuilder::GetCanonicalRegisterId(_In_ Object regObj)
{
    ULONG regId = (ULONG)regObj.KeyValue(L"Id");
    auto it = m_disRegToCanonical.find(regId);
    if (it == m_disRegToCanonical.end())
    {
        //
        // If we haven't seen this register yet, we need to look it up by NAME.
        //
        std::wstring regName = regObj.ToDisplayString();
        auto pSymManager = m_pFunction->InternalGetSymbolSet()->GetSymbolBuilderManager();

        RegisterInformation *pRegInfo;
        CheckHr(pSymManager->FindInformationForRegister(regName.c_str(), &pRegInfo));

        m_disRegToCanonical.insert( { regId, pRegInfo->Id });
        return pRegInfo->Id;
    }

    return it->second;
}

bool RangeBuilder::OperandToLocation(_In_ OperandInfo const& opInfo, _Out_ SvcSymbolLocation *pLocation)
{
    if ((opInfo.Flags & OperandMemory) != 0)
    {
        //
        // Only one register.  We cannot express more than one.  We cannot express one with a scaling factor.
        //
        if (opInfo.Regs[1] == NoRegister && opInfo.ScalingFactor == 1)
        {
            pLocation->Kind = SvcSymbolLocationRegisterRelative;
            pLocation->RegInfo.Number = opInfo.Regs[0];
            pLocation->RegInfo.Size = 16; // @TODO!!!
            
            if ((opInfo.Flags & OperandImmediate) != 0)
            {
                pLocation->Offset = static_cast<ULONG64>(opInfo.ConstantValue);
            }
            else
            {
                pLocation->Offset = 0;
            }

            return true;
        }
    }
    else if ((opInfo.Flags & OperandRegister) != 0)
    {
        //
        // Only one register.  We cannot express more than one.  We cannot express one with a scaling factor.
        //
        if (opInfo.Regs[1] == NoRegister && opInfo.ScalingFactor == 1 &&
            (opInfo.Flags & OperandImmediate) == 0)
        {
            pLocation->Kind = SvcSymbolLocationRegister;
            pLocation->RegInfo.Number = opInfo.Regs[0];
            pLocation->RegInfo.Size = 16; // @TODO!!!
            pLocation->Offset = 0;

            return true;
        }
    }

    return false;
}

bool RangeBuilder::UsesRegister(_In_ SvcSymbolLocation const& location, _In_ ULONG canonId)
{
    if (location.Kind == SvcSymbolLocationRegister || location.Kind == SvcSymbolLocationRegisterRelative)
    {
        return GetBaseRegister(location.RegInfo.Number) == GetBaseRegister(canonId);
    }
    return false;
}

bool RangeBuilder::CheckAddAlias(_In_ ULONG64 instrAddr,
                                 _In_ ULONG64 instrLen,
                                 _In_ OperandInfo const& outputInfo,
                                 _In_ OperandInfo const& inputInfo,
                                 _In_ LocationRange const& lr,
                                 _Inout_ ParameterRanges& ranges)
{
    SvcSymbolLocation inputLoc;
    SvcSymbolLocation outputLoc;
    if (OperandToLocation(inputInfo, &inputLoc) && 
        LocationsAreEquivalent(lr.ParamLocation.ParamLocation, inputLoc) &&
        OperandToLocation(outputInfo, &outputLoc))
    {
        ranges.push_back({ instrAddr + instrLen, instrAddr + instrLen, { outputLoc, 1 }, false });
        return true;
    }

    return false;
}

bool RangeBuilder::CheckForKill(_In_ OperandInfo const& opInfo, _In_ LocationRange const &lr)
{
    //
    // If the destination is a register and the live range uses that register, it is a kill.  Note that this
    // isn't necessarily a 'kill' as might be defined in the compiler.  It's really impossible for us to tell
    // the semantics of the write.  We cannot tell the semantic difference between some piece of code writing
    // back to the parameter:
    //
    //    int myfunc(int n /* rcx*/)
    //    {
    //        ...
    //        n = 42;   // mov rcx, 42
    //        ..
    //    }
    //
    // and the compiler having chosen to reuse the location for another semantic variable:
    //
    //    int mnfunc(int n /*rcx*/)
    //    {
    //        int j = n;    // mov rsi, rcx
    //        ...           // 'n' never used again
    //        ...           // compiler decides to reuse rcx for 'j'
    //        j = 42;       // mov rcx, 42
    //    }
    //
    // Thus, any write back to the location is treated as a 'kill'.  For memory locations, we are somewhat
    // more relaxed.
    //
    if ((opInfo.Flags & OperandOutput) != 0)
    {
        if ((opInfo.Flags & (OperandRegister | OperandMemory)) == OperandRegister)
        {
            if (UsesRegister(lr.ParamLocation.ParamLocation, opInfo.Regs[0]))
            {
                return true;
            }
        }
    }

    return false;
}

void RangeBuilder::GetOperandInfo(_In_ Object& operand, _Out_ OperandInfo *pOperandInfo)
{
    pOperandInfo->Flags = 0;

    for (size_t i = 0; i < ARRAYSIZE(pOperandInfo->Regs); ++i)
    {
        pOperandInfo->Regs[i] = NoRegister;
    }
    pOperandInfo->Regs[0] = NoRegister;
    pOperandInfo->ScalingFactor = 1;
    pOperandInfo->ConstantValue = 0;

    Object operandAttrs = operand.KeyValue(L"Attributes");

    bool isOutput = (bool)operandAttrs.KeyValue(L"IsOutput");
    bool isInput = (bool)operandAttrs.KeyValue(L"IsInput");
    bool isRegister = (bool)operandAttrs.KeyValue(L"IsRegister");
    bool isMemoryReference = (bool)operandAttrs.KeyValue(L"IsMemoryReference");
    bool hasImmediate = (bool)operandAttrs.KeyValue(L"HasImmediate");
    bool isImmediate = (bool)operandAttrs.KeyValue(L"IsImmediate");

    if (isOutput)
    {
        pOperandInfo->Flags |= OperandOutput;
    }
    if (isInput)
    {
        pOperandInfo->Flags |= OperandInput;
    }
    if (isRegister)
    {
        pOperandInfo->Flags |= OperandRegister;
    }
    if (isMemoryReference)
    {
        pOperandInfo->Flags |= OperandMemory;
    }
    if (isImmediate || hasImmediate)
    {
        pOperandInfo->Flags |= OperandImmediate;

        LONG64 immVal = (LONG64)operand.KeyValue(L"ImmediateValue");
        pOperandInfo->ConstantValue = immVal;
    }

    size_t regNum = 0;
    bool hasScale = false;

    Object opRegs = operand.KeyValue(L"Registers");
    for (auto&& regObj : opRegs)
    {
        if (regNum > ARRAYSIZE(pOperandInfo->Regs))
        {
            throw std::logic_error("Unexpected number of registers on operand");
        }

        ULONG canonId = GetCanonicalRegisterId(regObj);
        ULONG scaleFactor = (ULONG)regObj.KeyValue(L"ScaleFactor");

        pOperandInfo->Regs[regNum] = canonId;
        if (scaleFactor != 1)
        {
            if (hasScale)
            {
                throw std::logic_error("Unexpected multiple register scaling on operand");
            }

            pOperandInfo->ScalingFactor = scaleFactor;
            if (regNum != 0)
            {
                std::swap(pOperandInfo->Regs[0], pOperandInfo->Regs[regNum]);
            }
            hasScale = true;
        }
    }
}

bool RangeBuilder::CarryoverLiveRange(_In_ BasicBlockInfo& bbTo,
                                      _In_ size_t paramNum,
                                      _In_ LocationRange const& liveRange)
{
    bool matched = false;
    ULONG64 bbToStart = bbTo.StartAddress;

    ParameterRanges& pr = bbTo.BlockParameterRanges[paramNum];
    for (size_t i = 0; i < pr.size(); ++i)
    {
        LocationRange &lrTo = pr[i];
        if (lrTo.StartAddress == bbTo.StartAddress)
        {
            //
            // Is this the same...?
            //
            if (LocationsAreEquivalent(lrTo.ParamLocation.ParamLocation, liveRange.ParamLocation.ParamLocation))
            {
                matched = true;
                lrTo.ParamLocation.TraversalCount++;
            }
        }
    }

    if (!matched)
    {
        pr.push_back(
            { 
                bbTo.StartAddress,                  // [StartAddress, StartAddress) -- "empty" until traversed
                bbTo.StartAddress,
                { liveRange.ParamLocation.ParamLocation, 1 },
                false
            });
    }

    return !matched;
}

bool RangeBuilder::CarryoverLiveRanges(_In_ BasicBlockInfo& bbFrom, 
                                       _In_ BasicBlockInfo& bbTo,
                                       _In_ TraversalEntry const& entry)
{
    bool firstEntry = (bbTo.TraversalCount == 0);
    bool changedRanges = false;

    //
    // If this is the first entry into this basic block, initialize the parameter lists.
    //
    if (firstEntry)
    {
        bbTo.BlockParameterRanges.resize(bbFrom.BlockParameterRanges.size());
    }

    //
    // Find every parameter live range in bbFrom at entry.SourceBlockInstructionAddress and compare it against
    // what's already in bbTo.  If necessary, add or modify ranges in bbTo.
    //
    for (size_t p = 0; p < bbFrom.BlockParameterRanges.size(); ++p)
    {
        ParameterRanges& pr = bbFrom.BlockParameterRanges[p];
        for (size_t i = 0; i < pr.size(); ++i)
        {
            //
            // Does pr[i] include the one byte at entry.SourceBlockInstructionAddress...?
            //
            LocationRange const& lr = pr[i];
            if (lr.StartAddress <= entry.SourceBlockInstructionAddress &&
                lr.EndAddress > entry.SourceBlockInstructionAddress)
            {
                changedRanges = CarryoverLiveRange(bbTo, p, lr);
            }
        }
    }

    return (changedRanges && !firstEntry);
}

bool RangeBuilder::IsMov(_In_ std::wstring const& mnemonic)
{
    if (wcscmp(mnemonic.c_str(), L"mov") == 0) { return true; }
    return false;
}

void RangeBuilder::UpdateRangesForInstruction(_In_ BasicBlockInfo& block, _In_ Object instr)
{
    ULONG64 instrAddr = (ULONG64)instr.KeyValue(L"Address");
    ULONG64 instrLength = (ULONG64)instr.KeyValue(L"Length");

    Object instrAttrs = instr.KeyValue(L"Attributes");
    bool isCall = (bool)instrAttrs.KeyValue(L"IsCall");

    if (isCall)
    {
        //
        // A call is only guaranteed to preserve registers which are non-volatile by calling convention.
        // Go through any parameter register ranges which are currently live as of this instruction in this basic 
        // block and only carry forward ones which are held by non-volatiles.
        //
        for (size_t p = 0; p < block.BlockParameterRanges.size(); ++p)
        {
            ParameterRanges& pr = block.BlockParameterRanges[p];
            for (size_t i = 0; i < pr.size(); ++i)
            {
                LocationRange &lr = pr[i];
                if (!lr.Dead &&
                    (lr.ParamLocation.ParamLocation.Kind == SvcSymbolLocationRegister ||
                     lr.ParamLocation.ParamLocation.Kind == SvcSymbolLocationRegisterRelative))
                {
                    ULONG regRefId = lr.ParamLocation.ParamLocation.RegInfo.Number;
                    if (!m_pConvention->IsNonVolatile(regRefId))
                    {
                        //
                        // This range is now dead as of this instruction.  Do not carry it forward past the
                        // end of this instruction.
                        //
                        lr.Dead = true;
                    }
                    lr.EndAddress += instrLength;
                }
            }
        }
    }
    else
    {
        size_t numOperands = 0;
        OperandInfo oprnds[4];

        size_t numInputs = 0;
        OperandInfo *pInput = nullptr;
        size_t numOutputs = 0;
        OperandInfo *pOutput = nullptr;

        //
        // Unfortunately, the data model disassembler does not presently have a property that gets
        // us the instruction or mnemonic.  We need to grab this from the string conversion.
        //
        std::wstring instrStr = instr.ToDisplayString();
        wchar_t const *pc = instrStr.c_str();
        wchar_t const *pe = pc;
        while (*pe && !iswspace(*pe)) { ++pe; }
        std::wstring mnemonic(pc, pe - pc);

        bool isMov = IsMov(mnemonic);

        for (auto&& operand : instr.KeyValue(L"Operands"))
        {
            if (numOperands >= ARRAYSIZE(oprnds))
            {
                throw std::logic_error("Unexpected number of operands on instruction");
            }
            GetOperandInfo(operand, &oprnds[numOperands]);
            if ((oprnds[numOperands].Flags & OperandInput) != 0)
            {
                numInputs++;
                pInput = &(oprnds[numOperands]);
            }
            if ((oprnds[numOperands].Flags & OperandOutput) != 0)
            {
                numOutputs++;
                pOutput = &(oprnds[numOperands]);
            }

            numOperands++;
        }

        //
        // Look at all ranges that are presently live up to this instruction within this basic block 
        // and see if they are live after this instruction.
        //
        for (size_t p = 0; p < block.BlockParameterRanges.size(); ++p)
        {
            ParameterRanges& pr = block.BlockParameterRanges[p];
            for (size_t i = 0; i < pr.size(); ++i)
            {
                LocationRange &lr = pr[i];
                if (!lr.Dead)
                {
                    for (size_t o = 0; o < numOperands; ++o)
                    {
                        OperandInfo &opInfo = oprnds[o];
                        if (CheckForKill(opInfo, lr))
                        {
                            lr.Dead = true;
                        }
                    }
                    lr.EndAddress += instrLength;
                }
            }
        }

        //
        // Deal with any instruction level semantics...
        //
        if (numInputs == 1 && numOutputs == 1 && isMov)
        {
            for (size_t p = 0; p < block.BlockParameterRanges.size(); ++p)
            {
                ParameterRanges& pr = block.BlockParameterRanges[p];
                for (size_t i = 0; i < pr.size(); ++i)
                {
                    LocationRange &lr = pr[i];
                    if (instrAddr >= lr.StartAddress && instrAddr < lr.EndAddress)
                    {
                        CheckAddAlias(instrAddr, instrLength, *pOutput, *pInput, lr, pr);
                    }
                }
            }
        }
    }
}

void RangeBuilder::TraverseBasicBlock(_In_ TraversalEntry const& entry)
{
    //
    // We should already have traversed the basic block list, so nothing should ever "not be found"
    //
    auto itbbAddr = m_bbInfo.find(entry.BlockAddress);
    if (itbbAddr == m_bbInfo.end())
    {
        throw std::logic_error("Unexpected failure to find basic block");
    }
    BasicBlockInfo& bbInfo = itbbAddr->second;

    bool changedRanges = false;
    if (entry.SourceBlockAddress != 0)
    {
        auto itbbFrom = m_bbInfo.find(entry.SourceBlockAddress);
        if (itbbFrom == m_bbInfo.end())
        {
            throw std::logic_error("Unexpected failure to find basic block");
        }
        BasicBlockInfo& bbInfoFrom = itbbFrom->second;
        changedRanges = CarryoverLiveRanges(bbInfoFrom, bbInfo, entry);
    }

    bool firstTraversal = (bbInfo.TraversalCount == 0);
    ++bbInfo.TraversalCount;

    //
    // We only want to traverse this block if it's the first time we've hit this block *OR* there has been
    // some change in the list of live ranges entering the block from a different control flow.
    //
    if (firstTraversal || changedRanges)
    {
        //
        // Walk each instruction in the block and update live range information as appropriate based on
        // what the instructions are doing.
        //
        Object instrs = bbInfo.BasicBlock.KeyValue(L"Instructions");
        for (auto&& instr : instrs)
        {
            UpdateRangesForInstruction(bbInfo, instr);
        }

        //
        // Add each outbound control flow to the list of blocks to traverse (whether it is a fall through
        // flow, a branch flow, ...)
        //
        Object outboundFlows = bbInfo.BasicBlock.KeyValue(L"OutboundControlFlows");
        for (auto&& outboundFlow : outboundFlows)
        {
            Object destBlock = outboundFlow.KeyValue(L"LinkedBlock");
            Object linkageInstr = outboundFlow.KeyValue(L"SourceInstruction");
            ULONG64 linkageInstrAddr = (ULONG64)linkageInstr.KeyValue(L"Address");
            ULONG64 destAddr = (ULONG64)destBlock.KeyValue(L"StartAddress");
            m_bbTrav.push( { destAddr, bbInfo.StartAddress, linkageInstrAddr });
        }
    }
}

void RangeBuilder::InitializeParameterLocations(_In_ CallingConvention *pConvention, 
                                                _In_ BasicBlockInfo &entryBlock)
{
    std::vector<SvcSymbolLocation> entryLocations(m_parameters.size());

    pConvention->GetParameterPlacements(m_parameters.size(),
                                        &m_parameters[0],
                                        &entryLocations[0]);

    entryBlock.BlockParameterRanges.resize(m_parameters.size());
    for (size_t i = 0; i < m_parameters.size(); ++i)
    {
        ParameterRanges& ranges = entryBlock.BlockParameterRanges[i];
        ranges.push_back(
            { 
                entryBlock.StartAddress,                // [StartAddress, StartAddress) -- "empty" until traversed
                entryBlock.StartAddress,
                { entryLocations[i], 1 },
                false
            });
    }
}

void RangeBuilder::PropagateParameterRanges(_In_ FunctionSymbol *pFunction,
                                            _In_ CallingConvention *pConvention)
{
    m_bbInfo.clear();
    {
        decltype(m_bbTrav) emptyQueue;
        std::swap(m_bbTrav, emptyQueue);
    }
    m_parameters.clear();

    //
    // Build a quick index of the parameters of the function.  If there are none, we need do nothing.
    // This will require that we walk all the children of the function looking for parameters.
    //
    auto&& children = pFunction->InternalGetChildren();
    for (auto&& childId : children)
    {
        BaseSymbol *pSymbol = pFunction->InternalGetSymbolSet()->InternalGetSymbol(childId);
        if (pSymbol->InternalGetKind() != SvcSymbolDataParameter)
        {
            continue;
        }

        VariableSymbol *pParameter = static_cast<VariableSymbol *>(pSymbol);
        m_parameters.push_back(pParameter);
    }

    if (m_parameters.size() > 0)
    {
        m_pFunction = pFunction;
        m_pConvention = pConvention;
        CheckHr(pFunction->GetOffset(&m_functionOffset));
        CheckHr(pFunction->InternalGetSymbolSet()->GetModule()->GetBaseAddress(&m_modBase));

        Object disResult = m_dis.CallMethod(L"DisassembleFunction", m_modBase + m_functionOffset);
        Object bbs = disResult.KeyValue(L"BasicBlocks");

        //
        // Walk the basic block list and build our quick index.
        //
        for(auto&& bb : bbs)
        {
            ULONG64 startAddress = (ULONG64)bb.KeyValue(L"StartAddress");
            m_bbInfo.insert( { startAddress, bb } );
        }

        auto itbbFirst = m_bbInfo.find(m_modBase + m_functionOffset);
        if (itbbFirst == m_bbInfo.end())
        {
            throw std::runtime_error("Unable to find entry basic block to function");
        }
        InitializeParameterLocations(pConvention, itbbFirst->second);

        //
        // Start at the entry basic block and keep walking control flows until we reach a state where
        // we have no more control flows with different variable locations on entry.
        //
        m_bbTrav.push({ m_modBase + m_functionOffset, 0, 0 });
        while (!m_bbTrav.empty())
        {
            TraversalEntry entry = m_bbTrav.front();
            m_bbTrav.pop();
            TraverseBasicBlock(entry);
        }

        //
        // Move our built data over to the parameter symbol.
        //
        CreateLiveRangeSets();
    }
}

bool RangeBuilder::AddParameterRangeToFunction(_In_ size_t paramNum,
                                               _In_ ULONG64 &startAddress,
                                               _In_ ULONG64 &endAddress,
                                               _In_ SvcSymbolLocation const& location)
{
    VariableSymbol *pParam = m_parameters[paramNum];

    ULONG64 uniqueId;
    HRESULT hr = pParam->AddLiveRange(startAddress - m_modBase - m_functionOffset, 
                                      endAddress - startAddress,
                                      location,
                                      &uniqueId);

    //
    // Mark there as no "current range" from the caller's side.
    //
    startAddress = endAddress = 0;

    return SUCCEEDED(hr);
}

void RangeBuilder::CreateLiveRangeSets()
{
    //
    // We need to merge data from basic blocks so that:
    //
    //     - We never have contiguous ranges [A, B), [B, C).  This should merge into [A, C)
    //
    //     - We never have overlapping ranges.  We always pick one as the canonical representation of where
    //       the location is.  For instance, we may see a variable in @rcx and the following instructions:
    //
    //       1: mov rbx, rcx
    //       2: xor rcx, rcx
    //       3: ...
    //
    //       At (2), the variable is live in both rbx and rcx.  We must pick one.
    //
    // It makes this a bit easier if we first sort all the basic blocks by their start address.
    //
    std::vector<BasicBlockInfo *> bbsByAddress;
    for (auto&& kvp : m_bbInfo)
    {
        bbsByAddress.push_back(&kvp.second);
    }
    std::sort(bbsByAddress.begin(), bbsByAddress.end(),
              [&](_In_ BasicBlockInfo const* a, _In_ BasicBlockInfo const* b)
              {
                  return a->StartAddress < b->StartAddress;
              });

    for (size_t p = 0; p < m_parameters.size(); ++p)
    {
        ULONG64 instrp = bbsByAddress[0]->StartAddress;
        ULONG64 curRangeStart = 0;
        ULONG64 curRangeEnd = 0;
        SvcSymbolLocation curLocation;

        for (auto&& pbb : bbsByAddress)
        {
            BasicBlockInfo &bb = *pbb;
            ULONG traversalCount = bb.TraversalCount;

            auto&& pr = bb.BlockParameterRanges[p];

            //
            // Ranges may have gotten out of order linearly depending on how many control flows entered the 
            // block.  Sort them to make it easier to figure out.
            //
            std::sort(pr.begin(), pr.end(),
                      [&](_In_ const LocationRange& a, _In_ const LocationRange& b)
                      {
                          return a.StartAddress < b.StartAddress;
                      });

            while (instrp < bb.EndAddress)
            {
                //
                // Find the next range we're going to use.  We want a range that either covers 'instrp' or is above
                // instrp.  It cannot be one which ends *BELOW* 'instrp'.  Remember that everything is half-open, so
                // the range's EndAddress must be above 'instrp' for it to be useful to us.
                //
                LocationRange const *pLR = nullptr;
                for (size_t i = 0; i < pr.size(); ++i)
                {
                    //
                    // @TODO: For now, we are choosing to ignore control flow dependent locations.  In reality, if
                    //        there are no better options, we should be able to plumb this upward.
                    //
                    LocationRange const& lr = pr[i];
                    if (lr.ParamLocation.TraversalCount == traversalCount && lr.EndAddress > instrp &&
                        lr.EndAddress > lr.StartAddress)
                    {
                        pLR = &lr;
                        break;
                    }
                }

                //
                // Are there other ranges in this basic block that we need to deal with...?  Do we need to merge
                // this with an existing range...?
                //
                if (pLR != nullptr)
                {
                    if (curRangeStart == 0)
                    {
                        if (pLR->StartAddress >= instrp)
                        {
                            curRangeStart = pLR->StartAddress;
                        }
                        else
                        {
                            curRangeStart = instrp;
                        }
                        curRangeEnd = pLR->EndAddress;
                        curLocation = pLR->ParamLocation.ParamLocation;
                        instrp = pLR->EndAddress;
                    }
                    else
                    {
                        if (pLR->StartAddress >= curRangeStart &&
                            pLR->StartAddress <= curRangeEnd &&
                            pLR->EndAddress > curRangeEnd &&
                            LocationsAreEquivalent(curLocation, pLR->ParamLocation.ParamLocation))
                        {
                            curRangeEnd = pLR->EndAddress;
                            instrp = pLR->EndAddress;
                        }
                        else
                        {
                            //
                            // The range doesn't merge.  Add what we have and start a new range.
                            //
                            ULONG64 tempEnd = curRangeEnd;
                            (void)AddParameterRangeToFunction(p, curRangeStart, curRangeEnd, curLocation);

                            //
                            // Note that we cannot simply set instrp to pLR->StartAddress as pLR might
                            // overlap with our current range.  We only want the subset that does *NOT* overlap
                            //
                            if (pLR->StartAddress >= tempEnd)
                            {
                                instrp = pLR->StartAddress;
                            }
                            else
                            {
                                instrp = tempEnd;
                            }

                        }
                    }
                }
                else
                {
                    //
                    // We're done.
                    //
                    if (curRangeStart != 0)
                    {
                        (void)AddParameterRangeToFunction(p, curRangeStart, curRangeEnd, curLocation);
                    }
                    instrp = bb.EndAddress;
                }
            }
        }

        if (curRangeStart != 0)
        {
            (void)AddParameterRangeToFunction(p, curRangeStart, curRangeEnd, curLocation);
        }
    }
}

} // SymbolBuilder
} // Services
} // TargetComposition
} // Services

