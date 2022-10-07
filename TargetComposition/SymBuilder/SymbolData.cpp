//**************************************************************************
//
// SymbolData.cpp
//
// The implementation of data symbols within a "symbol set".  A "symbol set" is an
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

//*************************************************
// Base Data Symbols:
//

HRESULT BaseDataSymbol::BaseInitialize(_In_ SymbolSet *pSymbolSet,
                                       _In_ SvcSymbolKind symKind,
                                       _In_ ULONG64 owningTypeId,
                                       _In_ ULONG64 symOffset,
                                       _In_ ULONG64 symTypeId,
                                       _In_opt_ PCWSTR pwszName,
                                       _In_opt_ PCWSTR pwszQualifiedName)
{
    HRESULT hr = S_OK;

    //
    // This does not support the initialization of static fields (at present)
    //
    if (pwszQualifiedName != nullptr && symKind == SvcSymbolField)
    {
        return E_INVALIDARG;
    }

    //
    // Global data does not need an owning type.
    //
    BaseSymbol *pOwningType = nullptr;
    if (symKind != SvcSymbolData)
    {
        pOwningType = pSymbolSet->InternalGetSymbol(owningTypeId);
        if (pOwningType == nullptr || pOwningType->InternalGetKind() != SvcSymbolType)
        {
            return E_INVALIDARG;
        }
    }

    BaseSymbol *pSymbolType = pSymbolSet->InternalGetSymbol(symTypeId);
    if (pSymbolType == nullptr || pSymbolType->InternalGetKind() != SvcSymbolType)
    {
        return E_INVALIDARG;
    }

    m_rangeCacheOffset = Uninitialized;
    m_rangeCacheSize = Uninitialized;

    IfFailedReturn(BaseSymbol::BaseInitialize(pSymbolSet, symKind, owningTypeId, pwszName, pwszQualifiedName));

    m_symTypeId = symTypeId;
    m_symOffset = symOffset;

    if (pOwningType != nullptr)
    {
        IfFailedReturn(pOwningType->AddChild(InternalGetId()));
    }

    //
    // Keep track of the type's size and offset so we can recache information about the placement of the data upon
    // any change in the type.
    //
    BaseTypeSymbol *pType = static_cast<BaseTypeSymbol *>(pSymbolType);

    //
    // Let the symbol set know about the mapping of this symbol <-> address range if it has such.
    //
    if (symKind == SvcSymbolData && !InternalIsConstantValue())
    {
        ULONG64 start = InternalGetActualSymbolOffset();
        ULONG64 end = start + pType->InternalGetTypeSize();
        IfFailedReturn(pSymbolSet->InternalAddSymbolRange(start, end, InternalGetId()));
        m_rangeCacheOffset = start;
        m_rangeCacheSize = end - start;
    }

    //
    // Setup a chain of dependency:
    //
    //     - From the type of this field/base to the field itself
    //     - From the field/base to the owning type
    //
    // This way, changes will cause a recomputation of the layout of the owning type when:
    //
    //     - Something about the type of this field/base changes
    //     - Something about the field/base itself changes (e.g.: a manual change of type / offset / etc...)
    //
    IfFailedReturn(pSymbolType->AddDependentNotify(InternalGetId()));
    IfFailedReturn(AddDependentNotify(owningTypeId));

    return hr;
}

HRESULT BaseDataSymbol::BaseInitialize(_In_ SymbolSet *pSymbolSet,
                                       _In_ SvcSymbolKind symKind,
                                       _In_ ULONG64 owningTypeId,
                                       _In_ VARIANT *pValue,
                                       _In_ ULONG64 symTypeId,
                                       _In_ PCWSTR pwszName,
                                       _In_opt_ PCWSTR pwszQualifiedName)

{
    HRESULT hr = S_OK;

    //
    // Only fields and global data can initialize as a constant value!
    //
    if (symKind != SvcSymbolField && symKind != SvcSymbolData)
    {
        return E_INVALIDARG;
    }

    //
    // We can only deal with certain classes of variant types!
    //
    switch(pValue->vt)
    {
        case VT_EMPTY:
            //
            // A value of VT_EMPTY indicates that it is an automatic increment enumerant.  Make sure
            // the rest of it looks like an enumerant.
            //
            if (symTypeId != 0)
            {
                return E_INVALIDARG;
            }
            break;

        case VT_I1:
        case VT_I2:
        case VT_I4:
        case VT_I8:
        case VT_UI1:
        case VT_UI2:
        case VT_UI4:
        case VT_UI8:
        case VT_R4:
        case VT_R8:
        case VT_BOOL:
            break;

        default:
            return E_INVALIDARG;
    }

    // 
    // Global data does not need to belong to some type.
    //
    BaseSymbol *pOwningType = nullptr;
    if (symKind != SvcSymbolData)
    {
        pOwningType = pSymbolSet->InternalGetSymbol(owningTypeId);
        if (pOwningType == nullptr || pOwningType->InternalGetKind() != SvcSymbolType)
        {
            return E_INVALIDARG;
        }
    }

    BaseSymbol *pSymbolType = nullptr;
    if (symTypeId == 0)
    {
        //
        // Only enumerants are allowed to not have a specified type.  Such type will automatically inherit
        // from the parent enum type.  We must make sure in this case that the parent symbol really is an enum type!
        //
        BaseSymbol *pParentSymbol = pSymbolSet->InternalGetSymbol(owningTypeId);
        if (pParentSymbol == nullptr || pParentSymbol->InternalGetKind() != SvcSymbolType)
        {
            return E_INVALIDARG;
        }

        BaseTypeSymbol *pParentType = static_cast<BaseTypeSymbol *>(pParentSymbol);
        if (pParentType->InternalGetTypeKind() != SvcSymbolTypeEnum)
        {
            return E_INVALIDARG;
        }
    }
    else
    {
        pSymbolType = pSymbolSet->InternalGetSymbol(symTypeId);
        if (pSymbolType == nullptr || pSymbolType->InternalGetKind() != SvcSymbolType)
        {
            return E_INVALIDARG;
        }
    }

    IfFailedReturn(BaseSymbol::BaseInitialize(pSymbolSet, symKind, owningTypeId, pwszName, pwszQualifiedName));

    m_symTypeId = symTypeId;
    m_symOffset = (pValue->vt == VT_EMPTY ? UdtPositionalSymbol::AutomaticIncreaseConstantValue 
                                          : UdtPositionalSymbol::ConstantValue);

    //
    // NOTE: This is safe and we don't need to do anything like VariantCopy/VariantClear specifically because
    //       we only allow I1->I8, UI1->UI8, and R4->R8.
    //
    //       If this happens to have been VT_EMPTY, we will copy the "empty" notion until LayoutEnum completes.
    //
    m_symValue = *pValue;

    if (pOwningType != nullptr)
    {
        IfFailedReturn(pOwningType->AddChild(InternalGetId()));
    }

    //
    // Setup a chain of dependency:
    //
    //     - From the type of this field/base to the field itself
    //     - From the field/base to the owning type
    //
    // This way, changes will cause a recomputation of the layout of the owning type when:
    //
    //     - Something about the type of this field/base changes
    //     - Something about the field/base itself changes (e.g.: a manual change of type / offset / etc...)
    //
    if (pSymbolType != nullptr)
    {
        IfFailedReturn(pSymbolType->AddDependentNotify(InternalGetId()));
    }
    IfFailedReturn(AddDependentNotify(owningTypeId));

    return hr;
}

HRESULT BaseDataSymbol::Delete()
{
    HRESULT hr = S_OK;

    BaseSymbol *pSymbolType = InternalGetSymbolSet()->InternalGetSymbol(m_symTypeId);
    if (pSymbolType != nullptr)
    {
        //
        // Remove the chains of dependency introduced in our initializer.  We must remove both the chain from
        // the data type to us and our chain to the owning type.  Note that the latter will disappear automatically
        // when we are deleted.  In reality, we only need to remove the first here.
        //
        IfFailedReturn(pSymbolType->RemoveDependentNotify(InternalGetId()));
    }

    return BaseSymbol::Delete();
}

HRESULT BaseDataSymbol::GetType(_COM_Outptr_ ISvcSymbol **ppSymbolType)
{
    *ppSymbolType = nullptr;

    BaseSymbol *pSymbol = InternalGetSymbolSet()->InternalGetSymbol(m_symTypeId);
    if (pSymbol == nullptr)
    {
        return E_UNEXPECTED;
    }

    ComPtr<ISvcSymbol> spSymbol = pSymbol;
    *ppSymbolType = spSymbol.Detach();
    return S_OK;
}

HRESULT BaseDataSymbol::InternalSetSymbolOffset(_In_ ULONG64 symOffset)
{
    //
    // It's *MUCH* easier here if nothing changes.
    //
    if (symOffset == m_symOffset)
    {
        return S_OK;
    }

    m_symOffset = symOffset;

    HRESULT hr = NotifyDependentChange();

    // 
    // Send an advisory notification upwards that everyone should flush caches.  Do not consider this
    // a failure to create the symbol if something goes wrong.  At worst, an explicit .reload will be
    // required in the debugger.
    //
    (void)InternalGetSymbolSet()->InvalidateExternalCaches();

    return hr;
}

HRESULT BaseDataSymbol::InternalSetSymbolTypeId(_In_ ULONG64 symTypeId)
{
    //
    // It's *MUCH* easier here if nothing changes.
    //
    if (m_symTypeId == symTypeId)
    {
        return S_OK;
    }

    BaseSymbol *pNewSymType = InternalGetSymbolSet()->InternalGetSymbol(symTypeId);
    if (pNewSymType == nullptr || pNewSymType->InternalGetKind() != SvcSymbolType)
    {
        return E_INVALIDARG;
    }

    //
    // We need to remove certain chains of dependency and set up new ones.  At the end of the day,
    // this needs to be as if symTypeId was passed to our initializer.
    //
    HRESULT hr = S_OK;

    BaseSymbol *pCurSymType = InternalGetSymbolSet()->InternalGetSymbol(m_symTypeId);
    if (pCurSymType != nullptr)
    {
        IfFailedReturn(pCurSymType->RemoveDependentNotify(InternalGetId()));
    }

    IfFailedReturn(pNewSymType->AddDependentNotify(InternalGetId()));

    m_symTypeId = symTypeId;

    hr = NotifyDependentChange();

    // 
    // Send an advisory notification upwards that everyone should flush caches.  Do not consider this
    // a failure to create the symbol if something goes wrong.  At worst, an explicit .reload will be
    // required in the debugger.
    //
    (void)InternalGetSymbolSet()->InvalidateExternalCaches();

    return hr;
}

HRESULT BaseDataSymbol::NotifyDependentChange()
{
    HRESULT hr = S_OK;

    //
    // If something like the underlying type changed, recompute the size and, if that has changed, send
    // a notification to the symbol set so that it can recache symbol information.
    //
    if (m_symTypeId != 0 && InternalGetKind() == SvcSymbolData)
    {
        BaseSymbol *pSymbolType = InternalGetSymbolSet()->InternalGetSymbol(m_symTypeId);
        if (pSymbolType != nullptr && pSymbolType->InternalGetKind() == SvcSymbolType)
        {
            BaseTypeSymbol *pType = static_cast<BaseTypeSymbol *>(pSymbolType);
            ULONG64 typeSize = pType->InternalGetTypeSize();
            ULONG64 newStart = InternalGetActualSymbolOffset();

            if (m_rangeCacheOffset != Uninitialized && m_rangeCacheSize != Uninitialized &&
                (newStart != m_rangeCacheOffset || typeSize != m_rangeCacheSize))
            {
                IfFailedReturn(InternalGetSymbolSet()->InternalRemoveSymbolRange(m_rangeCacheOffset,
                                                                                 m_rangeCacheOffset + m_rangeCacheSize,
                                                                                 InternalGetId()));
            }

            if (newStart != m_rangeCacheOffset || typeSize != m_rangeCacheSize)
            {
                ULONG64 newEnd = newStart + typeSize;
                IfFailedReturn(InternalGetSymbolSet()->InternalAddSymbolRange(newStart, newEnd, InternalGetId()));
                m_rangeCacheOffset = newStart;
                m_rangeCacheSize = newEnd - newStart;
            }
        }
    }

    return BaseSymbol::NotifyDependentChange();
}

//*************************************************
// Global Data:
//

HRESULT GlobalDataSymbol::RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet,
                                                 _In_ ULONG64 parentId,
                                                 _In_ ULONG64 dataOffset,
                                                 _In_ ULONG64 dataTypeId,
                                                 _In_ PCWSTR pwszName,
                                                 _In_opt_ PCWSTR pwszQualifiedName)
{
    return BaseInitialize(pSymbolSet, SvcSymbolData, parentId, dataOffset, dataTypeId, pwszName, pwszQualifiedName);
}

} // SymbolBuilder
} // Services
} // TargetComposition
} // Debugger
