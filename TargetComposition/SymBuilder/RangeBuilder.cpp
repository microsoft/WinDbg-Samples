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
    auto pSymManager = m_pFunction->InternalGetSymbolSet()->GetSymbolBuilderManager();

    if ((opInfo.Flags & OperandMemory) != 0)
    {
        //
        // Only one register.  We cannot express more than one.  We cannot express one with a scaling factor.
        //
        if (opInfo.Regs[1] == NoRegister && opInfo.ScalingFactor == 1)
        {
            pLocation->Kind = SvcSymbolLocationRegisterRelative;
            pLocation->RegInfo.Number = opInfo.Regs[0];
            pLocation->RegInfo.Size = 8;                                // pointer-sized memory ref
            
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
            RegisterInformation *pRegInfo;
            CheckHr(pSymManager->FindInformationForRegisterById(opInfo.Regs[0], &pRegInfo));

            pLocation->Kind = SvcSymbolLocationRegister;
            pLocation->RegInfo.Number = opInfo.Regs[0];
            pLocation->RegInfo.Size = pRegInfo->Size;
            pLocation->Offset = 0;

            return true;
        }
    }

    return false;
}

bool RangeBuilder::UsesRegister(_In_ SvcSymbolLocation const& location, _In_ ULONG canonId)
{
    if (canonId != NoRegister &&
        (location.Kind == SvcSymbolLocationRegister || location.Kind == SvcSymbolLocationRegisterRelative))
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

    //
    // 1) Are we performing an aliasing of the location.  For exmaple, value is in @rcx and we do something
    //    like mov rdx, rcx.  This would alias rdx to rcx.
    //
    if (OperandToLocation(inputInfo, &inputLoc) && 
        LocationsAreEquivalent(lr.ParamLocation.ParamLocation, inputLoc) &&
        OperandToLocation(outputInfo, &outputLoc))
    {
        ranges.push_back({ instrAddr + instrLen, 
                           instrAddr + instrLen, 
                           { outputLoc, lr.ParamLocation.TraversalCountSlot }, 
                           LiveState::Live });
        return true;
    }

    //
    // 2) Are we changing a register value that would alias the underlying memory reference.  For instance
    //    a push rcx (which we translate into equivalent mov's for this call) would become something like
    //    mov rsp, rsp - 8.  In this case, the assignment of [rsp] would affect any memory reference using
    //    rsp.
    //
    else if (((outputInfo.Flags & OperandRegister) != 0) &&
             ((inputInfo.Flags & OperandImmediate) != 0) &&
             ((inputInfo.Flags & OperandMemory) == 0) &&
             outputInfo.Regs[0] == inputInfo.Regs[0] &&
             lr.ParamLocation.ParamLocation.Kind == SvcSymbolLocationRegisterRelative &&
             UsesRegister(lr.ParamLocation.ParamLocation, outputInfo.Regs[0]))
    {
        SvcSymbolLocation newLoc = lr.ParamLocation.ParamLocation;
        newLoc.Offset = static_cast<ULONG64>(static_cast<LONG64>(newLoc.Offset) - inputInfo.ConstantValue);
        ranges.push_back({ instrAddr + instrLen, 
                           instrAddr + instrLen, 
                           { newLoc, lr.ParamLocation.TraversalCountSlot }, 
                           LiveState::Live });
        return true;
    }

    //
    // 3) Are we aliasing one register to an offset of another register.  For example: lea r11, [rsp + 2b0]
    //    Note that we are treating such as a "mov r11, rsp + 2b0".  If we see a memory reference, this isn't
    //    the aliasing we think it is (e.g.: not the lea)
    //
    else if (((outputInfo.Flags & OperandRegister) != 0) &&
             ((inputInfo.Flags & (OperandRegister | OperandImmediate)) != 0) &&
             ((inputInfo.Flags & OperandMemory) == 0) &&
             lr.ParamLocation.ParamLocation.Kind == SvcSymbolLocationRegisterRelative &&
             inputInfo.Regs[0] != NoRegister && 
             UsesRegister(lr.ParamLocation.ParamLocation, inputInfo.Regs[0]))
    {
        SvcSymbolLocation newLoc;
        newLoc.Kind = SvcSymbolLocationRegisterRelative;
        newLoc.RegInfo.Number = outputInfo.Regs[0];
        newLoc.RegInfo.Size = 8;                                // pointer-sized memory ref
        newLoc.Offset = static_cast<ULONG64>(lr.ParamLocation.ParamLocation.Offset - inputInfo.ConstantValue);
        ranges.push_back({ instrAddr + instrLen, 
                           instrAddr + instrLen, 
                           { newLoc, lr.ParamLocation.TraversalCountSlot }, 
                           LiveState::Live });
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

    //
    // Anything which is an immediate operand is an input to the instruction's operation.  The data model
    // disassembler doesn't mark it that way currently.  Make sure we do.
    //
    if (isImmediate && !isInput && !isOutput)
    {
        pOperandInfo->Flags |= OperandInput;
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

void RangeBuilder::GetInstructionInfo(_In_ Object& instr, _Out_ InstructionInfo *pInstructionInfo)
{
    Object instrAttrs = instr.KeyValue(L"Attributes");

    pInstructionInfo->Address = (ULONG64)instr.KeyValue(L"Address");
    pInstructionInfo->Length = (ULONG64)instr.KeyValue(L"Length");
    pInstructionInfo->IsCall = (bool)instrAttrs.KeyValue(L"IsCall");

    //
    // Unfortunately, the data model disassembler does not presently have a property that gets
    // us the instruction or mnemonic.  We need to grab this from the string conversion.
    //
    std::wstring instrStr = instr.ToDisplayString();
    wchar_t const *pc = instrStr.c_str();
    wchar_t const *pe = pc;
    while (*pe && !iswspace(*pe)) { ++pe; }
    std::wstring mnemonic(pc, pe - pc);
    pInstructionInfo->Instr = GetRecognizedInstruction(mnemonic);
    
    pInstructionInfo->NumOperands = 0;

    for (auto&& operand : instr.KeyValue(L"Operands"))
    {
        if (pInstructionInfo->NumOperands >= ARRAYSIZE(pInstructionInfo->Operands))
        {
            throw std::logic_error("Unexpected number of operands on instruction");
        }
        GetOperandInfo(operand, &(pInstructionInfo->Operands[pInstructionInfo->NumOperands]));

        //
        // NOTE: There is a bug in the data model disassembler that the second operand for a LEA instruction
        //       is marked as neither input nor output.  We need to work around that in order to appropriately
        //       alias LEA references.
        //
        if (pInstructionInfo->Instr == RecognizedInstruction::Lea && pInstructionInfo->NumOperands == 1)
        {
            pInstructionInfo->Operands[pInstructionInfo->NumOperands].Flags |= OperandInput;
        }

        pInstructionInfo->NumOperands++;
    }
}

RangeBuilder::OperandInfo const *RangeBuilder::FindFirstInput(_In_ InstructionInfo const& instructionInfo)
{
    for (size_t i = 0; i < instructionInfo.NumOperands; ++i)
    {
        if ((instructionInfo.Operands[i].Flags & OperandInput) != 0)
        {
            return &instructionInfo.Operands[i];
        }
    }
    return nullptr;
}

RangeBuilder::OperandInfo const *RangeBuilder::FindFirstOutput(_In_ InstructionInfo const& instructionInfo)
{
    for (size_t i = 0; i < instructionInfo.NumOperands; ++i)
    {
        if ((instructionInfo.Operands[i].Flags & OperandOutput) != 0)
        {
            return &instructionInfo.Operands[i];
        }
    }
    return nullptr;
}

RangeBuilder::OperandInfo const *RangeBuilder::FindFirstImmediate(_In_ InstructionInfo const& instructionInfo)
{
    for (size_t i = 0; i < instructionInfo.NumOperands; ++i)
    {
        if ((instructionInfo.Operands[i].Flags & OperandImmediate) != 0 &&
            instructionInfo.Operands[i].Regs[0] == NoRegister)
        {
            return &instructionInfo.Operands[i];
        }
    }
    return nullptr;
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
                bbTo.TraversalCountSlots[lrTo.ParamLocation.TraversalCountSlot]++;
            }
        }
    }

    if (!matched)
    {
        bbTo.TraversalCountSlots.push_back(1);
        size_t traversalCountSlot = bbTo.TraversalCountSlots.size() - 1;

        pr.push_back(
            { 
                bbTo.StartAddress,                  // [StartAddress, StartAddress) -- "empty" until traversed
                bbTo.StartAddress,
                { liveRange.ParamLocation.ParamLocation, traversalCountSlot },
                LiveState::Live
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
                lr.EndAddress > entry.SourceBlockInstructionAddress &&
                lr.State == LiveState::LiveAtEndOfBlock)
            {
                changedRanges = CarryoverLiveRange(bbTo, p, lr);
            }
        }
    }

    return (changedRanges && !firstEntry);
}

RangeBuilder::RecognizedInstruction RangeBuilder::GetRecognizedInstruction(_In_ std::wstring const& mnemonic)
{
    if (wcscmp(mnemonic.c_str(), L"mov") == 0) { return RecognizedInstruction::Mov; }
    if (wcscmp(mnemonic.c_str(), L"push") == 0) { return RecognizedInstruction::Push; }
    if (wcscmp(mnemonic.c_str(), L"pop") == 0) { return RecognizedInstruction::Pop; }
    if (wcscmp(mnemonic.c_str(), L"add") == 0) { return RecognizedInstruction::Add; }
    if (wcscmp(mnemonic.c_str(), L"sub") == 0) { return RecognizedInstruction::Sub; }
    if (wcscmp(mnemonic.c_str(), L"lea") == 0) { return RecognizedInstruction::Lea; }
    else
    {
        return RecognizedInstruction::Unknown;
    }
}

void RangeBuilder::UpdateRangesForInstruction(_In_ BasicBlockInfo& block, _In_ Object instr)
{
    auto pSymbolSet = m_pFunction->InternalGetSymbolSet();
    auto pSymManager = pSymbolSet->GetSymbolBuilderManager();
    ULONG spId = m_pConvention->GetSpId();

    InstructionInfo curInstr;
    GetInstructionInfo(instr, &curInstr);

    if (curInstr.IsCall)
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
                if (lr.State == LiveState::Live &&
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
                        lr.State = LiveState::MarkedForKill;
                    }
                    lr.EndAddress += curInstr.Length;
                }
            }
        }

        //
        // @TODO: We need to ask something about how many caller pushed arguments the callee restores so we can
        //        adjust aliasing as needed for this.   For now, we simply assume there aren't any...  which means
        //        that stack referenced arguments will be wrong after a call which has stack based arguments that
        //        the callee pops.
        //
    }
    else
    {
        OperandInfo implicit;
        OperandInfo const *pImplicit = nullptr;
        OperandInfo const *pInput = FindFirstInput(curInstr);
        OperandInfo const *pOutput = FindFirstOutput(curInstr);
        OperandInfo const *pImmediate = FindFirstImmediate(curInstr);

        //
        // Are there implicit operands we need to deal with.  A "push rcx" instruction, for instance, will
        // only have "rcx" as an operand but there is an implicit write to "rsp" in doing so.
        //
        switch(curInstr.Instr)
        {
            case RecognizedInstruction::Push:
            case RecognizedInstruction::Pop:
                BuildOperand(spId, true, implicit);
                pImplicit = &implicit;
                break;
            default:
                break;
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
                if (lr.State == LiveState::Live)
                {
                    for (size_t o = 0; o < curInstr.NumOperands; ++o)
                    {
                        OperandInfo& opInfo = curInstr.Operands[o];
                        if (CheckForKill(opInfo, lr))
                        {
                            lr.State = LiveState::MarkedForKill;
                        }
                    }

                    //
                    // The "implicit operand" if any might cause a kill too.  For instance, a "push rcx" would
                    // implicitly alter rsp and kill any references that are at [rsp + N].  We would later recognize
                    // the creation of an alias to [rsp + N + M]
                    //
                    if (pImplicit != nullptr && CheckForKill(*pImplicit, lr))
                    {
                        lr.State = LiveState::MarkedForKill;
                    }

                    lr.EndAddress += curInstr.Length;
                }
            }
        }

        //
        // Deal with any instruction level semantics that might cause aliasing or other such semantics...
        //
        if (curInstr.Instr != RecognizedInstruction::Unknown)
        {
            for (size_t p = 0; p < block.BlockParameterRanges.size(); ++p)
            {
                bool handled = false;
                ParameterRanges& pr = block.BlockParameterRanges[p];
                for (size_t i = 0; i < pr.size(); ++i)
                {
                    LocationRange &lr = pr[i];
                    if (lr.State != LiveState::Dead && lr.State != LiveState::LiveAtEndOfBlock &&
                        curInstr.Address >= lr.StartAddress && curInstr.Address < lr.EndAddress)
                    {
                        OperandInfo op1, op2, op3;
                        
                        switch(curInstr.Instr)
                        {
                            case RecognizedInstruction::Mov:
                            {
                                CheckAddAlias(curInstr.Address, curInstr.Length, *pOutput, *pInput, lr, pr);
                                handled = true;
                                break;
                            }

                            case RecognizedInstruction::Lea:
                            {
                                //
                                // Slightly different semantic.  Instead of:
                                //
                                //     lea x, [y + z]
                                //
                                // Consider:
                                //
                                //     mov x, y + z
                                //
                                // And generate the appropriate aliasing for such.
                                //
                                op1 = *pInput;
                                op1.Flags &= ~OperandMemory;
                                op1.Flags |= OperandRegister | OperandImmediate;
                                CheckAddAlias(curInstr.Address, curInstr.Length, *pOutput, op1, lr, pr);
                                handled = true;
                                break;
                            }

                            case RecognizedInstruction::Push:
                            {
                                //
                                // Slightly different semantic.  Instead of:
                                //
                                //     push x
                                //
                                // Consider:
                                //
                                //     sub rsp, 8           ==> mov rsp, rsp - 8
                                //     mov [rsp], x
                                //
                                // And generate the appropriate aliasing for such
                                //
                                BuildOperand(spId, true, op1);                              // in: rsp
                                BuildOperand(spId, false, op2, -8);                         // in: rsp - 8
                                BuildOperand(spId, false, op3, 0, true);                    // out: [rsp]
                                CheckAddAlias(curInstr.Address, curInstr.Length, op1, op2, lr, pr);
                                CheckAddAlias(curInstr.Address, curInstr.Length, op3, *pInput, lr, pr);
                                handled = true;
                                break;
                            }

                            case RecognizedInstruction::Pop:
                            {
                                //
                                // Slightly different semantic.  Instead of:
                                //
                                //     pop x
                                //
                                // Consider:
                                //
                                //     mov x, [rsp]
                                //     add rsp, 8           ==> mov rsp, rsp + 8
                                //
                                BuildOperand(spId, false, op1, 0, true);                    // in: [rsp]
                                BuildOperand(spId, true, op2);                              // out: rsp
                                BuildOperand(spId, false, op3, 8);                          // in: rsp + 8
                                CheckAddAlias(curInstr.Address, curInstr.Length, *pOutput, op1, lr, pr);
                                CheckAddAlias(curInstr.Address, curInstr.Length, op2, op3, lr, pr);
                                handled = true;
                                break;
                            }

                            case RecognizedInstruction::Sub:
                            case RecognizedInstruction::Add:
                            {
                                //
                                // There are some very recognizable patterns around __chkstk which will seriously
                                // impact our ability to propagate parameter ranges without a deep understanding
                                // of the semantics of the method.  We will specially encode this hand-assembly 
                                // coded function that appears in many places and this pattern:
                                //
                                //     mov [eax/rax], <size>
                                //     call __chkstk
                                //     sub rsp, rax
                                //
                                // We will instead rewrite the sub instruction (for the purposes of aliasing) as:
                                //
                                //     sub rsp, <size>
                                //
                                // If we recognize this pattern and symbol.
                                //
                                if (pOutput && !pImmediate && pInput == pOutput &&  
                                    curInstr.NumOperands == 2 && 
                                    curInstr.Instr == RecognizedInstruction::Sub &&
                                    (curInstr.Operands[1].Flags & (OperandRegister | OperandInput | OperandMemory)) ==
                                        (OperandRegister | OperandInput) &&
                                    pOutput->Regs[0] == spId)
                                {
                                    //
                                    // At this point, we've seen a sub rsp, <register>.  Look for the pattern.
                                    //
                                    InstructionInfo *pNMinus1 = GetPreviousInstructionN(1);
                                    InstructionInfo *pNMinus2 = GetPreviousInstructionN(2);

                                    if (pNMinus1 && pNMinus2 &&
                                        pNMinus2->Address + pNMinus2->Length == pNMinus1->Address &&
                                        pNMinus1->Address + pNMinus1->Length == curInstr.Address &&
                                        pNMinus2->Instr == RecognizedInstruction::Mov &&
                                        pNMinus1->IsCall)
                                    {
                                        //
                                        // At this point, we have a recognized
                                        //
                                        // mov <reg1>, <size>
                                        // call <something>
                                        // sub rsp, <reg2>
                                        //
                                        OperandInfo const *pOutputNMinus2 = FindFirstOutput(*pNMinus2);
                                        OperandInfo const *pImmNMinus2 = FindFirstImmediate(*pNMinus2);
                                        OperandInfo const *pImmNMinus1 = FindFirstImmediate(*pNMinus1);
                                        if (pImmNMinus2 && pOutputNMinus2 && pImmNMinus1)
                                        {
                                            RegisterInformation *pSrc;
                                            RegisterInformation *pDest;
                                            CheckHr(pSymManager->FindInformationForRegisterById(pOutputNMinus2->Regs[0], &pSrc));
                                            CheckHr(pSymManager->FindInformationForRegisterById(curInstr.Operands[1].Regs[0], &pDest));

                                            if (pOutputNMinus2->Regs[0] == curInstr.Operands[1].Regs[0] ||
                                                GetBaseRegister(pOutputNMinus2->Regs[0]) == curInstr.Operands[1].Regs[0])
                                            {
                                                bool isChkStk = false;
                                                ULONG64 offs = pImmNMinus1->ConstantValue - m_modBase;
                                                ComPtr<ISvcSymbol> spSymbol;
                                                ULONG64 displacement;
                                                HRESULT hrSym = pSymbolSet->FindSymbolByOffset(offs,
                                                                                               true,
                                                                                               &spSymbol,
                                                                                               &displacement);
                                                if (SUCCEEDED(hrSym) && displacement == 0)
                                                {
                                                    BSTR symName;
                                                    if (SUCCEEDED(spSymbol->GetName(&symName)))
                                                    {
                                                        isChkStk = (wcscmp(symName, L"__chkstk") == 0);
                                                        SysFreeString(symName);
                                                    }
                                                }

                                                if (isChkStk)
                                                {
                                                    //
                                                    // We've recognized the pattern.  Substitute the <reg2> with
                                                    // the immediate from the mov <reg1>, <size> for the purposes
                                                    // of handling the sub.
                                                    //
                                                    op2 = *pImmNMinus2;
                                                    pImmediate = &op2;
                                                }
                                            }
                                        }
                                    }
                                }

                                //
                                // Slightly different semantic.  Instead of:
                                //
                                //     add x, y
                                //
                                // Consider:
                                //
                                //     mov x, x + y
                                //
                                // And generate the appropriate aliasing for such (assuming that 'y' is an
                                // immediate and 'x' is a register)
                                //
                                // Note that the first operand "x" will be Input | Output.  Find the immediate only
                                // operand if such exists.
                                //
                                if (pOutput && pImmediate && pInput == pOutput &&
                                    (pOutput->Flags & (OperandRegister | OperandMemory)) == OperandRegister)
                                {
                                    op1 = *pOutput;
                                    op1.ConstantValue = (curInstr.Instr == RecognizedInstruction::Add ? 
                                                             pImmediate->ConstantValue : -pImmediate->ConstantValue);
                                    op1.Flags |= OperandImmediate;
                                    CheckAddAlias(curInstr.Address, curInstr.Length, *pOutput, op1, lr, pr);
                                    handled = true;
                                }


                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    //
    // At the end of processing for this instruction, perform a final sweep and change any MarkedForKill
    // to a Dead state.  No further processing is ever required.
    //
    for (size_t p = 0; p < block.BlockParameterRanges.size(); ++p)
    {
        ParameterRanges& pr = block.BlockParameterRanges[p];
        for (size_t i = 0; i < pr.size(); ++i)
        {
            LocationRange &lr = pr[i];
            if (lr.State == LiveState::MarkedForKill)
            {
                lr.State = LiveState::Dead;
            }
        }
    }

    //
    // Update the processing window so that we can go back and deal with some particular patterns that might
    // be interesting.
    //
    m_processingWindow[m_processingWindowCur] = curInstr;
    m_processingWindowCur = (m_processingWindowCur + 1) % ARRAYSIZE(m_processingWindow);
    if (m_processingWindowSize < ARRAYSIZE(m_processingWindow))
    {
        ++m_processingWindowSize;
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

    if (++bbInfo.TraversalCount > MaximumTraversalCount)
    {
        throw std::runtime_error("Unable to propagate live ranges: maximum basic block traversal count exceeded");
    }

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
        // At the end of the basic block, mark every range as "live at end of block" so that a subsequent traversal 
        // inbound from another basic block does not attempt to update those ranges again.
        //
        for (size_t p = 0; p < bbInfo.BlockParameterRanges.size(); ++p)
        {
            ParameterRanges& pr = bbInfo.BlockParameterRanges[p];
            for (size_t i = 0; i < pr.size(); ++i)
            {
                LocationRange & lr = pr[i];
                if (lr.State == LiveState::Live)
                {
                    lr.State = LiveState::LiveAtEndOfBlock;
                }
            }
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

        entryBlock.TraversalCountSlots.push_back(1);
        size_t traversalCountSlot = entryBlock.TraversalCountSlots.size() - 1;

        ranges.push_back(
            { 
                entryBlock.StartAddress,                // [StartAddress, StartAddress) -- "empty" until traversed
                entryBlock.StartAddress,
                { entryLocations[i], traversalCountSlot },
                LiveState::Live
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

    m_processingWindowCur = m_processingWindowSize = 0;

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

        //
        // If there are existing live ranges on any of the parameters, they need to be deleted at this point.
        // Care needs to be taken to synchronize everything here properly.
        //
        for (auto&& pParam : m_parameters)
        {
            pParam->InternalDeleteAllLiveRanges();
        }

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
            auto&& tc = bb.TraversalCountSlots;

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
                    if (tc[lr.ParamLocation.TraversalCountSlot] == traversalCount && lr.EndAddress > instrp &&
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

