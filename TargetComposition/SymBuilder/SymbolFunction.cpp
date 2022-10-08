//**************************************************************************
//
// SymbolFunction.cpp
//
// The implementation of a function within a "symbol set".  A "symbol set" is an
// abstraction for the available symbols for a given module.  It is a set of
// "stacked" interfaces which implements progressively more functionality depending
// on the complexity of the symbol implementation.
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

HRESULT FunctionSymbol::RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet,
                                               _In_ ULONG64 parentId,
                                               _In_ ULONG64 returnType,
                                               _In_ ULONG64 codeOffset,
                                               _In_ ULONG64 codeSize,
                                               _In_ PCWSTR pwszName,
                                               _In_opt_ PCWSTR pwszQualifiedName)
{
    auto fn = [&]()
    {
        HRESULT hr = S_OK;

        m_returnType = returnType;

        IfFailedReturn(BaseInitialize(pSymbolSet, SvcSymbolFunction, parentId, pwszName, pwszQualifiedName));

        m_addressRanges.push_back(std::pair<ULONG64, ULONG64> { codeOffset, codeSize });

        IfFailedReturn(GetFunctionType(&m_functionType));

        IfFailedReturn(pSymbolSet->InternalAddSymbolRange(codeOffset, 
                                                          codeOffset + codeSize,
                                                          InternalGetId()));

        return hr;
    };
    return ConvertException(fn);
}

HRESULT FunctionSymbol::GetFunctionType(_Out_ ULONG64 *pFunctionTypeId)
{
    auto fn = [&]()
    {
        HRESULT hr = S_OK;

        //
        // @TODO: We should have a cache of these by signature in the symbol set so we aren't
        //        frequently recreating new "function type" symbols which are not reused.
        //
        std::vector<ULONG64> parameterTypes;

        auto&& children = InternalGetChildren();
        for(ULONG64 childId : children)
        {
            BaseSymbol *pChild = InternalGetSymbolSet()->InternalGetSymbol(childId);
            if (pChild == nullptr || pChild->InternalGetKind() != SvcSymbolDataParameter)
            {
                continue;
            }

            VariableSymbol *pParameterChild = static_cast<VariableSymbol *>(pChild);
            parameterTypes.push_back(pParameterChild->InternalGetSymbolTypeId());
        }

        ComPtr<FunctionTypeSymbol> spFunctionType;
        IfFailedReturn(MakeAndInitialize<FunctionTypeSymbol>(&spFunctionType, 
                                                             InternalGetSymbolSet(),
                                                             m_returnType,
                                                             parameterTypes.size(),
                                                             parameterTypes.size() == 0 ?
                                                                nullptr :
                                                                &(parameterTypes[0])));

        *pFunctionTypeId = spFunctionType->InternalGetId();
        return hr;
    };
    return ConvertException(fn);
}

HRESULT FunctionSymbol::GetType(_COM_Outptr_ ISvcSymbol **ppSymbolType)
{
    HRESULT hr = S_OK;
    *ppSymbolType = nullptr;

    BaseSymbol *pFunctionTypeSymbol = InternalGetSymbolSet()->InternalGetSymbol(m_functionType);
    if (pFunctionTypeSymbol == nullptr || pFunctionTypeSymbol->InternalGetKind() != SvcSymbolType)
    {
        return E_UNEXPECTED;
    }

    ComPtr<ISvcSymbol> spFunctionTypeSymbol = pFunctionTypeSymbol;
    *ppSymbolType = spFunctionTypeSymbol.Detach();
    return hr;
}

HRESULT FunctionSymbol::InternalSetReturnTypeId(_In_ ULONG64 returnTypeId)
{
    HRESULT hr = S_OK;

    //
    // It's *MUCH* easier here if nothing changes.
    //
    if (m_returnType == returnTypeId)
    {
        return S_OK;
    }

    BaseSymbol *pNewReturnType = InternalGetSymbolSet()->InternalGetSymbol(returnTypeId);
    if (pNewReturnType == nullptr || pNewReturnType->InternalGetKind() != SvcSymbolType)
    {
        return E_INVALIDARG;
    }

    //
    // Get a "function type" symbol for this particular function.  
    //
    // @TODO: It would be nice to be able to share these with functions with the same signature.
    //
    m_returnType = returnTypeId;
    IfFailedReturn(GetFunctionType(&m_functionType));

    // 
    // Send an advisory notification upwards that everyone should flush caches.  Do not consider this
    // a failure to create the symbol if something goes wrong.  At worst, an explicit .reload will be
    // required in the debugger.
    //
    (void)InternalGetSymbolSet()->InvalidateExternalCaches();

    return hr;
}

} // SymbolBuilder
} // Services
} // TargetComposition
} // Debugger
