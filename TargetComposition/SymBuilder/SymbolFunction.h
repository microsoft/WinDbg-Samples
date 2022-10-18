//**************************************************************************
//
// SymbolFunction.h
//
// The header for our implementation of a function within a "symbol set".  A "symbol set" is an
// abstraction for the available symbols for a given module.  It is a set of
// "stacked" interfaces which implements progressively more functionality depending
// on the complexity of the symbol implementation.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#ifndef __SYMBOLFUNCTION_H__
#define __SYMBOLFUNCTION_H__

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace SymbolBuilder
{

// FunctionSymbol:
//
// Class for the private symbol representation of a function.
//
class FunctionSymbol :
    public Microsoft::WRL::RuntimeClass<
        BaseSymbol
        >
{
public:

    //*************************************************
    // ISvcSymbol:
    //

    // GetOffset():
    //
    // Gets the offset of the symbol (if said symbol has such).  Note that if the symbol has multiple
    // disjoint address ranges associated with it, this method may return S_FALSE to indicate that the
    // symbol does not necessarily have a simple "base address" for an offset.
    //
    IFACEMETHOD(GetOffset)(_Out_ ULONG64 *pSymbolOffset)
    {
        *pSymbolOffset = m_addressRanges[0].first;

        //
        // If there are disjoint ranges, we return S_FALSE as an indicator of this
        //
        return (m_addressRanges.size() == 1 ? S_OK : S_FALSE);
    }

    //*************************************************
    // ISvcSymbolInfo:
    //

    // GetType():
    //
    // Gets the type of the symbol.
    //
    IFACEMETHOD(GetType)(_COM_Outptr_ ISvcSymbol **ppSymbolType);

    // GetLocation():
    //
    // Gets the location of the symbol.
    //
    IFACEMETHOD(GetLocation)(_Out_ SvcSymbolLocation *pLocation)
    {
        pLocation->Kind = SvcSymbolLocationImageOffset;
        pLocation->Offset = m_addressRanges[0].first;

        //
        // If there are disjoint ranges, we return S_FALSE as an indicator of this.
        //
        return (m_addressRanges.size() == 1 ? S_OK : S_FALSE);
    }

    //*************************************************
    // Internal APIs:
    //

    virtual HRESULT NotifyDependentChange()
    {
        //
        // If one of the parameters changes, we need to recompute the function type.
        //
        HRESULT hr = GetFunctionType(&m_functionType);
        if (SUCCEEDED(hr))
        {
            hr = BaseSymbol::NotifyDependentChange();
        }
        return hr;
    }

    HRESULT RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet,
                                   _In_ ULONG64 parentId,
                                   _In_ ULONG64 returnType,
                                   _In_ ULONG64 codeOffset,
                                   _In_ ULONG64 codeSize,
                                   _In_ PCWSTR pwszName,
                                   _In_opt_ PCWSTR pwszQualifiedName);

    //*************************************************
    // Internal Accessors:
    //

    virtual ULONG64 InternalGetReturnTypeId() const { return m_returnType; }
    std::vector<std::pair<ULONG64, ULONG64>> const& InternalGetAddressRanges() const { return m_addressRanges; }

    //*************************************************
    // Internal  Setters:
    //

    virtual HRESULT InternalSetReturnTypeId(_In_ ULONG64 returnTypeId);

private:

    // GetFunctionType():
    //
    // Gets a function type symbol for the type of the function.
    //
    HRESULT GetFunctionType(_Out_ ULONG64 *pFunctionTypeId);

    // The set of address ranges associated with this function.  The first range is
    // considered the "primary" range including the entry point of the function.  Many
    // functions will have a single code range.  It is, however, possible that due to
    // optimizations, there are disjoint code ranges associated with the function.  In this
    // case, m_addressRanges will hold more than one entry.
    //
    // Each pair entry is offset, size resulting in a half open range of [first, first + second)
    //
    std::vector<std::pair<ULONG64, ULONG64>> m_addressRanges;

    ULONG64 m_functionType;
    ULONG64 m_returnType;

};

} // SymbolBuilder
} // Services
} // TargetComposition
} // Debugger

#endif // __SYMBOLFUNCTION_H__
