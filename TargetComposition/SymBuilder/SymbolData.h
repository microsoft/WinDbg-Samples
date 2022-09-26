//**************************************************************************
//
// SymbolData.h
//
// The header for our implementation of global data within a "symbol set".  A "symbol set" is an
// abstraction for the available symbols for a given module.  It is a set of
// "stacked" interfaces which implements progressively more functionality depending
// on the complexity of the symbol implementation.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#ifndef __SYMBOLDATA_H__
#define __SYMBOLDATA_H__

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace SymbolBuilder
{

// BaseDataSymbol:
//
// Base class for a variety of data symbols (e.g.: fields, enumerants, global variables, etc...)
//
class BaseDataSymbol : 
    public Microsoft::WRL::Implements<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        BaseSymbol
        >
{
public:

    // ConstantValue:
    //
    // A constant used for the field offsets which indicates that this field has a constant value and
    // no other location.
    //
    static constexpr ULONG64 ConstantValue = static_cast<ULONG64>(-1ll);

    //*************************************************
    // ISvcSymbol:
    //

    // GetValue()
    //
    // Gets the value of a constant value symbol.  GetLocation will return an indication that the symbol
    // has a constant value.
    //
    // If this method is called on a symbol without a constant value, it will fail.
    //
    IFACEMETHOD(GetValue)(_Out_ VARIANT *pValue)
    {
        if (!InternalIsConstantValue())
        {
            return E_NOTIMPL;
        }

        //
        // NOTE: This does *NOT* require a VariantCopy or the like because we only support a very limited
        //       subset of variant values.
        //
        *pValue = m_symValue;
        return S_OK;
    }

    // GetOffset():
    //
    // Gets the offset of the symbol (if said symbol has such).  Note that if the symbol has multiple
    // disjoint address ranges associated with it, this method may return S_FALSE to indicate that the symbol
    // does not necessarily have a simple "base address" for an offset.
    //
    IFACEMETHOD(GetOffset)(_Out_ ULONG64 *pSymbolOffset)
    {
        *pSymbolOffset = m_symOffset;
        return S_OK;
    }

    //*************************************************
    // ISvcSymbolInfo:
    //

    // GetType():
    //
    // Gets the type of the symbol.
    //
    IFACEMETHOD(GetType)(_COM_Outptr_ ISvcSymbol **ppSymbolType);

    // GetLocation()
    //
    // Gets the location of the symbol.
    //
    IFACEMETHOD(GetLocation)(_Out_ SvcSymbolLocation *pLocation)
    {
        //
        // If the field has a constant value, indicate this.
        //
        if (InternalIsConstantValue())
        {
            pLocation->Kind = SvcSymbolLocationConstantValue;
            return S_OK;
        }

        //
        // The offset for global data is relative to the base of the module (where it loaded).  The offset
        // for a field is relative to the beginning of the structure (unless it is static).
        //
        pLocation->Kind = (InternalGetKind() == SvcSymbolData ? SvcSymbolLocationImageOffset :
                                                                SvcSymbolLocationStructureRelative);

        pLocation->Offset = InternalGetActualSymbolOffset();
        return S_OK;
    }

    //*************************************************
    // Internal APIs:
    //

    // BaseInitialize():
    //
    // Initialize the data symbol (as an offset based symbol).  This offset is either relative to some structure
    // (e.g.: it is a field) or is relative to the base address of the module (e.g.: it is global data)
    //
    HRESULT BaseInitialize(_In_ SymbolSet *pSymbolSet,
                           _In_ SvcSymbolKind symKind,
                           _In_ ULONG64 owningTypeId,
                           _In_ ULONG64 symOffset,
                           _In_ ULONG64 symTypeId,
                           _In_opt_ PCWSTR pwszName,
                           _In_opt_ PCWSTR pwszQualifiedName);

    // BaseInitialize():
    //
    // Initialize the data symbol (as a value based symbol).  Only fields and global data can initialize
    // in this way!
    //
    HRESULT BaseInitialize(_In_ SymbolSet *pSymbolSet,
                           _In_ SvcSymbolKind symKind,
                           _In_ ULONG64 owningTypeId,
                           _In_ VARIANT *pValue,
                           _In_ ULONG64 symTypeId,
                           _In_ PCWSTR pwszName,
                           _In_opt_ PCWSTR pwszQualifiedName);

    // Delete():
    //
    // Called when the data symbol is deleted.
    //
    virtual HRESULT Delete();

    // NotifyDependentChange():
    //
    // Called when something this symbol is dependent upon changes (e.g.: layout, etc...).  If our underlying
    // type changed, we need to refetch the size and subsequently pass a notification to the symbol set so that
    // it can update its mapping of symbol <-> offset.
    //
    virtual HRESULT NotifyDependentChange();

    //*************************************************
    // Internal Accessors:
    //

    virtual bool InternalIsConstantValue() const { return m_symOffset == ConstantValue; }
    virtual bool InternalHasType() const { return m_symTypeId != 0; }

    virtual ULONG64 InternalGetSymbolTypeId() const { return m_symTypeId; }
    virtual ULONG64 InternalGetSymbolOffset() const { return m_symOffset; }
    virtual ULONG64 InternalGetActualSymbolOffset() const { return m_symOffset; }
    virtual VARIANT const& InternalGetSymbolValue() const { return m_symValue; }

    //*************************************************
    // Internal  Setters:
    //

    virtual HRESULT InternalSetSymbolTypeId(_In_ ULONG64 symTypeId);
    virtual HRESULT InternalSetSymbolOffset(_In_ ULONG64 symOffset);

protected:


    ULONG64 m_symTypeId;                // What is the **SYMBOL'S** type (enumerants will not have this)
    ULONG64 m_symOffset;                // Either hard coded or ConstantValue (derived classes may add to this)
    VARIANT m_symValue;                 // Relevant only for constant valued fields

    //
    // Caches for symbol ranges:
    //

    // Uninitialized:
    //
    // Indicates that a cached value is uninitialized.
    //
    static constexpr ULONG64 Uninitialized = static_cast<ULONG64>(-1ll);

    ULONG64 m_rangeCacheOffset;         // How the symbol's range map is currently registered: base offset
    ULONG64 m_rangeCacheSize;           // How the symbol's range map is currently registered: size
};

// GlobalDataSymbol:
//
// Represents global data within a module (e.g.: a global variable)
//
class GlobalDataSymbol :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        BaseDataSymbol
        >
{
public:

    //*************************************************
    // Internal APIs:
    //

    HRESULT RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet,
                                   _In_ ULONG64 parentId,
                                   _In_ ULONG64 dataOffset,
                                   _In_ ULONG64 dataTypeId,
                                   _In_ PCWSTR pwszName,
                                   _In_opt_ PCWSTR pwszQualifiedName);


};

} // SymbolBuilder
} // Services
} // TargetComposition
} // Debugger

#endif // __SYMBOLDATA_H__
