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

class BaseScope;

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
        //
        // Function locals and parameters have more complex locations than a simple offset.
        // We need to get location through a scope/scope frame.
        //
        if (InternalGetKind() == SvcSymbolDataParameter || InternalGetKind() == SvcSymbolDataLocal)
        {
            return E_FAIL;
        }
                   
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
        switch(InternalGetKind())
        {
            case SvcSymbolData:
                pLocation->Kind = SvcSymbolLocationImageOffset;
                break;

            case SvcSymbolDataParameter:
            case SvcSymbolDataLocal:
                //
                // You need to go through a scope/scope frame to get the more complex notions of
                // location that vary by instruction within the function.
                //
                return E_FAIL;

            default:
                pLocation->Kind = SvcSymbolLocationStructureRelative;
            
        }

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
                           _In_ ULONG64 owningSymbolId,
                           _In_ ULONG64 symOffset,
                           _In_ ULONG64 symTypeId,
                           _In_opt_ PCWSTR pwszName,
                           _In_opt_ PCWSTR pwszQualifiedName,
                           _In_ ULONG64 bitFieldLength = 0,
                           _In_ ULONG64 bitFieldPosition = 0,
                           _In_ bool newSymbol = true,
                           _In_ ULONG64 id = 0);

    // BaseInitialize():
    //
    // Initialize the data symbol (as a value based symbol).  Only fields and global data can initialize
    // in this way!
    //
    HRESULT BaseInitialize(_In_ SymbolSet *pSymbolSet,
                           _In_ SvcSymbolKind symKind,
                           _In_ ULONG64 owningSymbolId,
                           _In_ VARIANT *pValue,
                           _In_ ULONG64 symTypeId,
                           _In_ PCWSTR pwszName,
                           _In_opt_ PCWSTR pwszQualifiedName,
                           _In_ bool newSymbol = true,
                           _In_ ULONG64 id = 0);

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
    virtual bool InternalIsBitField() const { return m_bitFieldLength != 0; }
    virtual bool InternalHasType() const { return m_symTypeId != 0; }

    virtual ULONG64 InternalGetSymbolTypeId() const { return m_symTypeId; }
    virtual ULONG64 InternalGetSymbolOffset() const { return m_symOffset; }
    virtual ULONG64 InternalGetActualSymbolOffset() const { return m_symOffset; }
    virtual VARIANT const& InternalGetSymbolValue() const { return m_symValue; }
    virtual ULONG64 InternalGetBitFieldLength() const { return m_bitFieldLength; }
    virtual ULONG64 InternalGetBitFieldPosition() const { return m_bitFieldPosition; }
    virtual ULONG64 InternalGetActualBitFieldPosition() const { return m_bitFieldPosition; }

    //*************************************************
    // Internal  Setters:
    //

    virtual HRESULT InternalSetSymbolTypeId(_In_ ULONG64 symTypeId);
    virtual HRESULT InternalSetSymbolOffset(_In_ ULONG64 symOffset);
    virtual HRESULT InternalSetSymbolValue(_In_ VARIANT const* pVal);
    virtual HRESULT InternalSetBitFieldLength(_In_ ULONG64 bitFieldLength);
    virtual HRESULT InternalSetBitFieldPosition(_In_ ULONG64 bitFieldPosition);

protected:

    //*************************************************
    // Helpers:
    //

    // CanBeBitField():
    //
    // Returns whether or not this particular symbol can be a bitfield (its type is compatible with this).
    //
    HRESULT CanBeBitField(_Out_ bool *pCanBeBitfield, _Out_ ULONG64 *pTypeSize);

    ULONG64 m_symTypeId;                // What is the **SYMBOL'S** type (enumerants will not have this)
    ULONG64 m_symOffset;                // Either hard coded or ConstantValue (derived classes may add to this)
    ULONG64 m_bitFieldLength;           // Zero indicates not a bitfield; non-zero indicates a bitfield
    ULONG64 m_bitFieldPosition;         // Either hard coded or ConstantValue (derived classes may add to this)
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

// VariableSymbol:
//
// Represents a variable (parameter / local) within a function.
//
class VariableSymbol :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        BaseDataSymbol
        >
{
public:

    // LiveRange:
    //
    // Describes a range of the owning function where this variable is live.
    //
    struct LiveRange
    {
        ULONG64 UniqueId;                       // Unique identifier for the range within this var
        ULONG64 Offset;                         // Function relative offset of the range
        ULONG64 Size;                           // Size of the range
        SvcSymbolLocation VariableLocation;     // Where the var is within this range
    };

    //*************************************************
    // ISvcSymbol:
    //

    // GetOffset():
    //
    // Gets the offset of the symbol (if said symbol has such).  Note that if the symbol has multiple
    // disjoint address ranges associated with it, this method may return S_FALSE to indicate that the symbol
    // does not necessarily have a simple "base address" for an offset.
    //
    IFACEMETHOD(GetOffset)(_Out_ ULONG64 *pSymbolOffset);

    //*************************************************
    // ISvcSymbolInfo:
    //

    // GetLocation()
    //
    // Gets the location of the symbol.
    //
    IFACEMETHOD(GetLocation)(_Out_ SvcSymbolLocation *pLocation);

    //*************************************************
    // Internal APIs:
    //

    HRESULT RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet,
                                   _In_ SvcSymbolKind symKind,
                                   _In_ ULONG64 parentId,
                                   _In_ ULONG64 parameterTypeId,
                                   _In_ PCWSTR pwszName);

    HRESULT RuntimeClassInitialize(_In_ VariableSymbol *pSourceSymbol,
                                   _In_ BaseScope *pBoundScope);

    // ValidateLiveRange():
    //
    // Validates that a live range of [rangeOffset, rangeOffset + rangeSize) is valid in that
    // it does not extend outside the bounds of the function or overlap with another live range.
    // Note that if ignoreRange is a range id (non-zero), that range will be ignored for the live
    // range overlap check.
    //
    bool ValidateLiveRange(_In_ ULONG64 rangeOffset,
                           _In_ ULONG64 rangeSize,
                           _In_ ULONG64 ignoreRange = 0);

    // AddLiveRange():
    //
    // Adds varLocation as the location of this variable within the half open function relative
    // address range [rangeOffset, rangeOffset + rangeSize).  This will return a unique handle to the
    // live range if it succeeds.
    //
    HRESULT AddLiveRange(_In_ ULONG64 rangeOffset,
                         _In_ ULONG64 rangeSize,
                         _In_ SvcSymbolLocation const& varLocation,
                         _Out_ ULONG64 *pUniqueId);

    // BindToScope():
    //
    // Binds this variable to a particular scope (or scope frame) and thus a location within
    // a function.  This allows the location returning APIs to return the particular location for
    // this variable at this place in the function.
    //
    HRESULT BindToScope(_In_ BaseScope *pScope,
                        _COM_Outptr_ VariableSymbol **ppBoundVariable)
    {
        *ppBoundVariable = nullptr;
        return Microsoft::WRL::MakeAndInitialize<VariableSymbol>(ppBoundVariable,
                                                                 this,
                                                                 pScope);
    }

    // MoveToBefore():
    //
    // Moves this *PARAMETER* symbol to before another one in order.  This rearranges the containing
    // function's parameter list.
    //
    HRESULT MoveToBefore(_In_ ULONG64 position);

    // GetLiveRange():
    //
    // Gets the live range for a given unique id.
    //
    LiveRange *GetLiveRange(_In_ ULONG64 uniqueId) 
    {
        auto it = m_liveRanges.find(uniqueId);
        if (it != m_liveRanges.end())
        {
            return &(it->second);
        }

        return nullptr;
    }

    // GetLiveRangeByOffset():
    //
    // Gets the appropriate live range for this variable by the given scope(function) relative
    // offset.
    //
    LiveRange const *GetLiveRangeByOffset(_In_ ULONG64 srelOffset) const
    {
        for (LiveRange *pRange : m_liveRangeList)
        {
            if (srelOffset >= pRange->Offset && srelOffset < (pRange->Offset + pRange->Size))
            {
                return pRange;
            }
        }

        return nullptr;
    }

    std::vector<LiveRange *> const &InternalGetLiveRanges() const { return m_liveRangeList; }
    bool IsBoundToScope() const { return m_spBoundScope.Get() != nullptr; }
    BaseScope *GetBoundScope() const;

    bool InternalSetLiveRangeOffset(_In_ ULONG64 id, _In_ ULONG64 offset);
    bool InternalSetLiveRangeSize(_In_ ULONG64 id, _In_ ULONG64 size);
    bool InternalSetLiveRangeLocation(_In_ ULONG64 id, _In_ SvcSymbolLocation const& location);
    bool InternalDeleteLiveRange(_In_ ULONG64 id);
    void InternalDeleteAllLiveRanges();

private:

    ULONG64 m_curId;
    std::unordered_map<ULONG64, LiveRange> m_liveRanges;
    std::vector<LiveRange *> m_liveRangeList;
    Microsoft::WRL::ComPtr<ISvcSymbolSetScope> m_spBoundScope;
};

} // SymbolBuilder
} // Services
} // TargetComposition
} // Debugger

#endif // __SYMBOLDATA_H__
