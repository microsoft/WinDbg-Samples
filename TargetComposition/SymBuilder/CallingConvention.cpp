//**************************************************************************
//
// CallingConvention.cpp
//
// The implementation for our understanding of calling conventions.
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
                                     _In_reads_(numNonVolatiles) wchar_t const **ppNonVolatileNames) :
    m_pManager(pManager)
{
    for (size_t i = 0; i < numNonVolatiles; ++i)
    {
        RegisterInformation *pRegInfo;
        CheckHr(pManager->FindInformationForRegister(ppNonVolatileNames[i], &pRegInfo));
        m_nonVolatiles.insert( { pRegInfo->Id } );
    }
}

void CallingConvention::FillRegisterCanonicalIds(_In_ size_t numRegisters,
                                                 _In_reads_(numRegisters) wchar_t const **ppRegNames,
                                                 _Out_writes_(numRegisters) ULONG *pRegIds)
{
    for (size_t i = 0; i < numRegisters; ++i)
    {
        RegisterInformation *pRegInfo;
        CheckHr(m_pManager->FindInformationForRegister(ppRegNames[i], &pRegInfo));
        pRegIds[i] = pRegInfo->Id;
    }
}

bool CallingConvention::IsNonVolatile(_In_ ULONG canonId)
{
    //
    // If we are passed a sub-register (e.g.: 'al', 'ax', 'eax' of 'rax'), walk it up until we find the base register.
    //
    for(;;)
    {
        RegisterInformation *pRegInfo;
        CheckHr(m_pManager->FindInformationForRegisterById(canonId, &pRegInfo));
        if (pRegInfo->ParentId != static_cast<ULONG>(-1))
        {
            canonId = pRegInfo->ParentId;
        }
        else
        {
            break;
        }
    }
    
    auto it = m_nonVolatiles.find(canonId);
    return (it != m_nonVolatiles.end());
}

//*************************************************
// AMD64 Calling Convention Understanding:
//

wchar_t const *g_AMD64_win_nonvolatiles[] =
{
    L"r12", L"r13", L"r14", L"r15", L"rdi", L"rsi", L"rbx", L"rbp", L"rsp"
};

wchar_t const *g_AMD64_win_ordinalparams[] =
{
    L"rcx", L"rdx", L"r8", L"r9"
};

wchar_t const *g_AMD64_win_floatparams[] =
{
    L"xmm0", L"xmm1", L"xmm2", L"xmm3"
};

CallingConvention_Windows_AMD64::CallingConvention_Windows_AMD64(_In_ SymbolBuilderManager *pManager) :
    CallingConvention(pManager, 
                      ARRAYSIZE(g_AMD64_win_nonvolatiles), g_AMD64_win_nonvolatiles)
{
    FillRegisterCanonicalIds(ARRAYSIZE(g_AMD64_win_ordinalparams), g_AMD64_win_ordinalparams, m_ordIds);
    FillRegisterCanonicalIds(ARRAYSIZE(g_AMD64_win_floatparams), g_AMD64_win_floatparams, m_fltIds);
    
    RegisterInformation *pSpInfo;
    CheckHr(m_pManager->FindInformationForRegister(L"rsp", &pSpInfo));
    m_spId = pSpInfo->Id;
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

    //
    // On entry into the callee, rsp points to the return address.  rsp + 8 would point to the first stack based
    // parameter
    //
    ULONG64 stackOffset = 8;
    for (size_t i = 0; i < paramCount; ++i)
    {
        VariableSymbol *pParameter = ppParameters[i];
        ULONG64 symTypeId = pParameter->InternalGetSymbolTypeId();

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
                symTypeId = pTypedefType->InternalGetTypedefOfTypeId();
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

            bool canEnregister = true;

            ULONG regBase;
            ULONG64 maxRegSz = 8;
            if (ordinal && i < m_ordIds.size())
            {
                regBase = m_ordIds[i];
            }
            else if (!ordinal && i < m_fltIds.size())
            {
                regBase = m_fltIds[i];
                maxRegSz = 16;
            }
            else
            {
                canEnregister = false;
            }

            if (canEnregister)
            {
                //
                // If the struct does not fit into a register, by calling convention, a pointer to the register
                // will be placed.  If someone put struct in the symbol builder's debug info, generate a live
                // range which is register relative.
                //
                pLocations[i].RegInfo.Number = regBase;
                pLocations[i].RegInfo.Size = 8;

                if (tySz <= maxRegSz)
                {
                    pLocations[i].Kind = SvcSymbolLocationRegister;

                    //
                    // Is this a sub-register portion...?
                    //
                    if (tySz < maxRegSz)
                    {
                        //
                        // Sub-registering is always going to be a power-of-two size.  Find the closest power-of-two
                        // that fits the given type.
                        //
                        ULONG64 rsz = maxRegSz;
                        while (rsz > 0 && (rsz / 2) >= tySz) { rsz /= 2; }

                        RegisterInformation *pParentInfo;
                        CheckHr(m_pManager->FindInformationForRegisterById(regBase, &pParentInfo));

                        for (size_t sr = 0; sr < pParentInfo->SubRegisters.size(); ++sr)
                        {
                            RegisterInformation *pSubRegister;
                            CheckHr(m_pManager->FindInformationForRegisterById(pParentInfo->SubRegisters[sr],
                                                                               &pSubRegister));

                            if (pSubRegister->Size == rsz && pSubRegister->SubLsb == 0)
                            {
                                
                                pLocations[i].RegInfo.Number = pSubRegister->Id;
                                pLocations[i].RegInfo.Size = static_cast<ULONG>(rsz);
                                break;
                            }
                        }
                    }

                } else {
                    pLocations[i].Kind = SvcSymbolLocationRegisterRelative;
                    pLocations[i].RegInfo.Size = 8;
                }

                pLocations[i].Offset = 0;
            }
            else
            {
                //
                // It's on the stack.
                //
                pLocations[i].Kind = SvcSymbolLocationRegisterRelative;
                pLocations[i].RegInfo.Number = m_spId;
                pLocations[i].RegInfo.Size = 8;
                pLocations[i].Offset = stackOffset;

                //
                // The stack must stay 8 byte aligned regardless of the parameter size.
                //
                ULONG64 alignSz = (tySz + 7) & ~7;
                stackOffset += alignSz;
            }

            break;
        }
    }
}

} // SymbolBuilder
} // Services
} // TargetComposition
} // Debugger

