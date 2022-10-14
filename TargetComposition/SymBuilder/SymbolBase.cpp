//**************************************************************************
//
// SymbolBase.cpp
//
// The implementation of base symbols within a "symbol set".  A "symbol set" is an
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
// Base Symbols:
//

HRESULT BaseSymbol::InitializeNewSymbol(_In_ ULONG64 reservedId)
{
    return m_pSymbolSet->AddNewSymbol(this, &m_id, reservedId);
}

HRESULT BaseSymbol::RemoveChild(_In_ ULONG64 uniqueId)
{
    //
    // We cannot let a C++ exception escape.
    //
    auto fn = [&]()
    {
        HRESULT hr = S_OK;

        bool found = false;
        for (auto it = m_children.begin(); it != m_children.end(); ++it)
        {
            if (*it == uniqueId)
            {
                m_children.erase(it);
                found = true;
                break;
            }
        }

        if (found)
        {
            return NotifyDependentChange();
        }

        return hr;
    };
    return ConvertException(fn);
}

HRESULT BaseSymbol::MoveChildBefore(_In_ ULONG64 childId, 
                                    _In_ ULONG64 pos,
                                    _In_ SvcSymbolKind relativeTo)
{
    auto fn = [&]()
    {
        if (pos > std::numeric_limits<size_t>::max())
        {
            return E_INVALIDARG;
        }

        //
        // Find where the child is currently at in the list of children and then find its new position
        // depending on whether it is absolute or relative.
        //
        size_t idx = 0;
        while (idx < m_children.size() && m_children[idx] != childId)
        {
            ++idx;
        }

        if (idx >= m_children.size())
        {
            return E_INVALIDARG;
        }

        //
        // If the positioning is absolute, pos is the new position; otherwise, we must count how many symbols 
        // of type 'relativeTo' have occurred to find the appropriate index.
        //
        size_t newIdx;
        if (relativeTo == SvcSymbol)
        {
            newIdx = static_cast<size_t>(pos);
        }
        else
        {
            newIdx = 0;
            ULONG64 count = 0;
            while (newIdx < m_children.size())
            {
                BaseSymbol *pChild = InternalGetSymbolSet()->InternalGetSymbol(m_children[newIdx]);
                if (pChild != nullptr && pChild->InternalGetKind() == relativeTo)
                {
                    if (count == pos)
                    {
                        break;
                    }

                    ++count;
                }

                ++newIdx;
            }
        }

        if (newIdx > idx) { --newIdx; }
        m_children.erase(m_children.begin() + idx);
        m_children.insert(m_children.begin() + newIdx, childId);

        // 
        // Changing the position of a field may actually change the size of the type due to alignment and packing.
        // This means anyone dependent on this symbol must recompute their layouts.  We need to pass this notification
        // onward.
        //
        // We must also send an advisory notification upwards that everyone should flush caches.  Do not consider this
        // a failure to create the symbol if something goes wrong.  At worst, an explicit .reload will be
        // required in the debugger.
        //
        HRESULT hr = NotifyDependentChange();
        (void)InternalGetSymbolSet()->InvalidateExternalCaches();
        return hr;
    };
    return ConvertException(fn);
}

HRESULT BaseSymbol::EnumerateChildren(_In_ SvcSymbolKind kind,
                                      _In_opt_z_ PCWSTR pwszName,
                                      _In_opt_ SvcSymbolSearchInfo * /*pSearchInfo*/,
                                      _COM_Outptr_ ISvcSymbolSetEnumerator **ppChildEnum)
{
    HRESULT hr = S_OK;
    *ppChildEnum = nullptr;

    ComPtr<ChildEnumerator> spEnum;
    IfFailedReturn(MakeAndInitialize<ChildEnumerator>(&spEnum, this, kind, pwszName));

    *ppChildEnum = spEnum.Detach();
    return hr;
}

HRESULT BaseSymbol::NotifyDependentChange()
{
    HRESULT hr = S_OK;
    for (auto&& kvp : m_dependentNotifySymbols)
    {
        ULONG64 uniqueId = kvp.first;
        BaseSymbol *pNotifySymbol = InternalGetSymbolSet()->InternalGetSymbol(uniqueId);
        if (pNotifySymbol != nullptr)
        {
            hr = pNotifySymbol->NotifyDependentChange();
            if (FAILED(hr))
            {
                return hr;
            }
        }
    }

    return hr;
}

HRESULT BaseSymbol::Delete()
{
    HRESULT hr = S_OK;
    for (auto&& child : m_children)
    {
        HRESULT hrChild = S_OK;

        BaseSymbol *pSymbol = InternalGetSymbolSet()->InternalGetSymbol(child);
        if (pSymbol != nullptr)
        {
            hrChild = (pSymbol->Delete());
        }

        if (FAILED(hrChild) && SUCCEEDED(hr))
        {
            hr = hrChild;
        }
    }
    m_children.clear();

    BaseSymbol *pParentSymbol = InternalGetSymbolSet()->InternalGetSymbol(m_parentId);
    if (pParentSymbol != nullptr)
    {
        HRESULT hrParent = pParentSymbol->RemoveChild(InternalGetId());
        if (FAILED(hrParent) && SUCCEEDED(hr))
        {
            hr = hrParent;
        }
    }

    HRESULT hrDelete = InternalGetSymbolSet()->DeleteExistingSymbol(InternalGetId());
    if (FAILED(hrDelete) && SUCCEEDED(hr))
    {
        hr = hrDelete;
    }

    return hr;
}

HRESULT ChildEnumerator::GetNext(_COM_Outptr_ ISvcSymbol **ppSymbol)
{
    *ppSymbol = nullptr;

    auto&& children = m_pSymbol->InternalGetChildren();

    while (m_pos < children.size())
    {
        BaseSymbol *pBaseSymbol = m_pSymbol->InternalGetSymbolSet()->InternalGetSymbol(children[m_pos]);
        ++m_pos;

        if (pBaseSymbol == nullptr)
        {
            //
            // Something has gone *SERIOUSLY* wrong if we have a child id that no longer happens to be indexed
            // by the symbol set!
            //
            return E_UNEXPECTED;
        }

        //
        // Do we have any additional match criteria...?
        //
        //     - Are we searching for a specific symbol kind or any symbol...?
        //     - Are we searching for a specific symbol by name...?
        //
        if (m_kind != SvcSymbol && m_kind != pBaseSymbol->InternalGetKind())
        {
            continue;
        }

        if (!m_name.empty() && m_name != pBaseSymbol->InternalGetName())
        {
            continue;
        }

        ComPtr<ISvcSymbol> spSymbol = pBaseSymbol;
        *ppSymbol = spSymbol.Detach();
        return S_OK;
    }

    return E_BOUNDS;
}

//*************************************************
// Publics:
//

HRESULT PublicSymbol::RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet,
                                             _In_ ULONG64 offset,
                                             _In_ PCWSTR pwszName,
                                             _In_opt_ PCWSTR pwszQualifiedName)
{
    HRESULT hr = S_OK;
    IfFailedReturn(BaseInitialize(pSymbolSet, SvcSymbolPublic, 0, pwszName, pwszQualifiedName));

    m_offset = offset;
    IfFailedReturn(pSymbolSet->InternalAddPublicSymbol(offset, InternalGetId()));

    return hr;
}

} // SymbolBuilder
} // Services
} // TargetComposition
} // Debugger
