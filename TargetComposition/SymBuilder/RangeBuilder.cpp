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

using namespace Microsoft::WRL;

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace SymbolBuilder
{

// 
// NOTE: We are going to encode the *register names* of non-volatile and other registers.  We will go ask
//       the architecture service to map these to canonical IDs rather than embedding canonical numberings
//       here.  Bear in mind as well that the disassembler has its own domain specific numbering which does 
//       *NOT* align.  It will return names along with the IDs so everything can be correlated thorugh such.
//
//       @TODO: At some point, this should deal with MMX/AVX registers for floating point types.
//

CallingConvention::CallingConvention(_In_ SymbolBuilderManager *pManager,
                                     _In_ size_t numNonVolatiles,
                                     _In_reads_(numNonVolatiles) wchar_t const **ppNonVolatileNames)
{
    for (size_t i = 0; i < numNonVolatiles; ++i)
    {
        RegisterInformation *pRegInfo;
        CheckHr(pManager->FindInformationForRegister(ppNonVolatileNames[i], &pRegInfo));
        m_nonVolatiles.insert( { pRegInfo->Id } );
    }
}

//*************************************************
// AMD64 Calling Convention Understanding:
//

wchar_t const *g_AMD64_win_nonvolatiles[] =
{
    L"r12", L"r13", L"r14", L"r15", L"rdi", L"rsi", L"rbx", L"rbp", L"rsp"
};

wchar_t const *g_AMD64_win_ordinalparams[] =
    L"rcx", L"rdx", L"r8", L"r9"
};

whcar_t const *g_AMD64_win_floatparams[] =
    L"xmm0", "xmm1", "xmm2", "xmm3"
};

CallingConvention_Windows_AMD64::CallingConvention_Windows_AMD64(_In_ SymbolBuilderManager *pManager) :
    CallingConvention(pManager, 
                      ARRAYSIZE(g_AMD64_win_nonvolatiles), g_AMD64_win_nonvolatiles,
                      ARRAYSIZE(g_AMD64_win_ordinalparamregs), g_AMD64_win_ordinalparamregs)
{
}

void CallingConvention_Windows_AMD64::GetParameterPlacements(_In_ size_t paramCount,
                                                             _In_reads_(paramCount) VariableSymbol **ppParameters,
                                                             _Out_writes_(paramCount) SvcSymbolLocation *pLocations)
{
    //
    // Pre-initialize everything to "no location".  That's simply what we'll do if we presently don't understand
    // where it should go.
    //
    for (size_t i = 0; i < paramCount; ++i)
    {
        pLocations[i].Kind = SvcSymbolLocationNone;
    }

    ULONG64 stackOffset = 0;
    for (size_t i = 0; i < paramCount; ++i)
    {
        VariableSymbol *pParameter = ppParameters[i];
        ULONG symTypeId = pParameter->InternalGetSymbolTypeId();

        //
        // If it is a typedef, we need to unwind the typedef and continue.  The outer loop here is 
        // explicitly for that.
        //
        for (;;)
        {
            BaseSymbol *pParamTypeSym = pParameter->InternalGetSymbolSet()->InternalGetSymbol(symTypeId);
            if (pParamTypeSym == nullptr || pParamTypeSym->InternalGetKind() != SvcSymbolType)
            {
                break;
            }

            BaseTypeSymbol *pParamType = static_cast<BaseTypeSymbol *>(pParamTypeSym);

            SvcSymbolTypeKind tyk = pParamType->InternalGetTypeKind();
            ULONG64 tySz = pParamType->InternalGetTypeSize();

            //
            // If it's a typedef, we really need to unwind it to understand where things go.
            //
            if (tyk == SvcSymbolTypeTypedef)
            {
                TypedefTypeSymbol *pTypedefType = static_cast<TypedefTypeSymbol *>(pParamType);
                symTypeId = pTypedefType->InternalGetTypedefOfType();
                continue;
            }

            //
            // Is it ordinal or is it a floating point value that goes in xmm* ...?
            //
            bool ordinal = true;
            if (tyk == SvcSymbolTypeIntrinsic)
            {
                BasicTypeSymbol *pBasicType = static_cast<BasicTypeSymbol *>(pParamType);
                SvcSymbolIntrinsicKind tyik = pBasicType->InternalGetIntrinsicKind();
                if (tyik == SvcSymbolIntrinsicFloat)
                {
                    ordinal = false;
                }
            }








    }
}

//*************************************************
// ARM64 Calling Convention Understnading:
//

wchar_t const *g_ARM64_win_nonvolatiles[] =
{
    L"x18", L"x19", L"x20", L"x21", L"x22", L"x23", L"x24", L"x25", L"x26", L"x27", L"x28", L"x29", L"x30"
};

wchar_t const *g_ARM64_win_ordinalparamregs[] =
{

};

CallingConvention_Windows_ARM64::CallingConvention_Windows_ARM64(_In_ SymbolBuilderManager *pManager) :
    CallingConvention(pManager, ARRAYSIZE(g_ARM64_win_nonvolatiles), g_ARM64_win_nonvolatiles)
{
}

//*************************************************
// General Range Builder:
//

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

void RangeBuilder::TraverseBlockAt(_In_ ULONG64 bbAddr, _In_ ULONG64 bbFrom)
{
    //
    // We should already have traversed the basic block list, so nothing should ever "not be found"
    //
    auto itbbAddr = m_bbInfo.find(bbAddr);
    if (itbbAddr == m_bbInfo.end())
    {
        throw std::logic_error("Unexpected failure to find basic block");
    }
    BasicBlockInfo& bbInfo = *itbbAddr;

    bool changedRanges = false;
    if (bbFrom != 0)
    {
        auto itbbFrom = m_bbInfo.find(bbFrom);
        if (itbbFrom == m_bbInfo.end())
        {
            throw std::logic_error("Unexpected failure to find basic block");
        }
        BasicBlockInfo& bbInfoFrom = *itbbFrom;
        changedRanges = CarryoverLiveRanges(bbInfo, bbInfoFrom);
    }

    //
    // We only want to traverse this block if it's the first time we've hit this block *OR* there has been
    // some change in the list of live ranges entering the block from a different control flow.
    //
    if (bbInfo.TraversalCount == 0 || changedRanges)
    {
        ++bbInfo.TraversalCount;

        //
        // Walk each instruction in the block and update live range information as appropriate based on
        // what the instructions are doing.
        //
        Object instrs = bbInfo.BasicBlock.KeyValue(L"Instructions");
        for (auto&& instr : instrs)
        {
        }

        //
        // Add each outbound control flow to the list of blocks to traverse (whether it is a fall through
        // flow, a branch flow, ...)
        //
        Object outboundFlows = m_bbInfo.BasicBlock.KeyValue(L"OutboundControlFlows");
        for (auto&& outboundFlow : outboundFlows)
        {
            Object destBlock = outboundFlow.KeyValue(L"LinkedBlock");
            ULONG64 destAddr = (ULONG64)destBlock.KeyValue(L"StartAddress");
            m_bbTrav.push_back( { destAddr, bbAddr });
        }
    }
}

void RangeBuilder::PropagateParameterRanges(_In_ FunctionSymbol *pFunction,
                                            _In_ CallingConvention *pConvention)
{
    m_bbInfo.clear();
    m_bbTrav.clear();
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

        m_bbTrav.push_back({ m_modBase + m_functionOffset, 0 });

        while (!m_bbTrav.empty())
        {
            std::pair<ULONG64, ULONG64> addrAndFrom = m_bbTrav.front();
            m_bbTrav.pop();
            TraverseBasicBlockAt(addrAndFrom.first, addrAndFrom.second);
        }
    }
}

} // SymbolBuilder
} // Services
} // TargetComposition
} // Services

