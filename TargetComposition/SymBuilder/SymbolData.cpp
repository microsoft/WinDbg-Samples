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
                                       _In_ ULONG64 owningSymbolId,
                                       _In_ ULONG64 symOffset,
                                       _In_ ULONG64 symTypeId,
                                       _In_opt_ PCWSTR pwszName,
                                       _In_opt_ PCWSTR pwszQualifiedName,
                                       _In_ ULONG64 bitFieldLength,
                                       _In_ ULONG64 bitFieldPosition,
                                       _In_ bool newSymbol,
                                       _In_ ULONG64 id)
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
    BaseSymbol *pOwningSymbol = nullptr;
    if (symKind != SvcSymbolData)
    {
        pOwningSymbol = pSymbolSet->InternalGetSymbol(owningSymbolId);
        if (pOwningSymbol == nullptr || 
            (pOwningSymbol->InternalGetKind() != SvcSymbolType && symKind != SvcSymbolDataParameter &&
             symKind != SvcSymbolDataLocal))
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

    IfFailedReturn(BaseSymbol::BaseInitialize(pSymbolSet, symKind, owningSymbolId, pwszName, pwszQualifiedName, newSymbol, id));

    m_symTypeId = symTypeId;
    m_symOffset = symOffset;
    
    //
    // If the request is for a bitfield, make sure the values make sense and that the type supports such!
    //
    if (bitFieldLength != 0)
    {
        bool canBeBitField;
        ULONG64 typeSize;
        IfFailedReturn(CanBeBitField(&canBeBitField, &typeSize));

        if (!canBeBitField || bitFieldLength > typeSize * 8)
        {
            return E_INVALIDARG;
        }

        if ((symOffset == UdtPositionalSymbol::AutomaticAppendLayout) !=
            (bitFieldPosition == UdtPositionalSymbol::AutomaticAppendLayout))
        {
            return E_INVALIDARG;
        }

        if (bitFieldPosition != UdtPositionalSymbol::AutomaticAppendLayout &&
            bitFieldPosition + bitFieldLength > typeSize * 8)
        {
            return E_INVALIDARG;
        }
    }

    m_bitFieldLength = bitFieldLength;
    m_bitFieldPosition = bitFieldPosition;

    if (pOwningSymbol != nullptr && newSymbol)
    {
        IfFailedReturn(pOwningSymbol->AddChild(InternalGetId()));
    }

    if (symKind != SvcSymbolDataParameter && symKind != SvcSymbolDataLocal)
    {
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
        if (newSymbol)
        {
            IfFailedReturn(pSymbolType->AddDependentNotify(InternalGetId()));
        }
    }

    if (newSymbol)
    {
        IfFailedReturn(AddDependentNotify(owningSymbolId));
    }

    return hr;
}

HRESULT BaseDataSymbol::BaseInitialize(_In_ SymbolSet *pSymbolSet,
                                       _In_ SvcSymbolKind symKind,
                                       _In_ ULONG64 owningSymbolId,
                                       _In_ VARIANT *pValue,
                                       _In_ ULONG64 symTypeId,
                                       _In_ PCWSTR pwszName,
                                       _In_opt_ PCWSTR pwszQualifiedName,
                                       _In_ bool newSymbol,
                                       _In_ ULONG64 id)

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
    BaseSymbol *pOwningSymbol = nullptr;
    if (symKind != SvcSymbolData)
    {
        pOwningSymbol = pSymbolSet->InternalGetSymbol(owningSymbolId);
        if (pOwningSymbol == nullptr || 
            (pOwningSymbol->InternalGetKind() != SvcSymbolType && symKind != SvcSymbolDataParameter &&
             symKind != SvcSymbolDataLocal))
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
        BaseSymbol *pParentSymbol = pSymbolSet->InternalGetSymbol(owningSymbolId);
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

    IfFailedReturn(BaseSymbol::BaseInitialize(pSymbolSet, symKind, owningSymbolId, pwszName, pwszQualifiedName, newSymbol, id));

    m_symTypeId = symTypeId;
    m_symOffset = (pValue->vt == VT_EMPTY ? UdtPositionalSymbol::AutomaticIncreaseConstantValue 
                                          : UdtPositionalSymbol::ConstantValue);
    m_bitFieldLength = 0;
    m_bitFieldPosition = UdtPositionalSymbol::AutomaticAppendLayout;

    //
    // NOTE: This is safe and we don't need to do anything like VariantCopy/VariantClear specifically because
    //       we only allow I1->I8, UI1->UI8, and R4->R8.
    //
    //       If this happens to have been VT_EMPTY, we will copy the "empty" notion until LayoutEnum completes.
    //
    m_symValue = *pValue;

    if (newSymbol)
    {
        if (pOwningSymbol != nullptr)
        {
            IfFailedReturn(pOwningSymbol->AddChild(InternalGetId()));
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
        if (symKind != SvcSymbolDataParameter && symKind != SvcSymbolDataLocal)
        {
            if (pSymbolType != nullptr)
            {
                IfFailedReturn(pSymbolType->AddDependentNotify(InternalGetId()));
            }
        }

        IfFailedReturn(AddDependentNotify(owningSymbolId));
    }

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

    if (m_rangeCacheOffset != Uninitialized && m_rangeCacheSize != Uninitialized)
    {
        InternalGetSymbolSet()->InternalRemoveSymbolRange(m_rangeCacheOffset, 
                                                          m_rangeCacheSize, 
                                                          InternalGetId());
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

    //
    // If this field happens to be a bitfield, we may need to propagate data into the bitfield 
    // values when we switch from auto-layout to manual-layout or vice-versa.
    //
    if (InternalIsBitField() &&
            (symOffset == UdtPositionalSymbol::AutomaticAppendLayout) !=
            (m_symOffset == UdtPositionalSymbol::AutomaticAppendLayout))
    {
        if (symOffset == UdtPositionalSymbol::AutomaticAppendLayout)
        {
            m_bitFieldPosition = symOffset;
        }
        else
        {
            m_bitFieldPosition = InternalGetActualBitFieldPosition();
        }
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

HRESULT BaseDataSymbol::CanBeBitField(_Out_ bool *pCanBeBitField, _Out_ ULONG64 *pTypeSize)
{
    HRESULT hr = S_OK;

    ULONG64 typeId = m_symTypeId;
    for(;;)
    {
        BaseSymbol *pTypeSymbol = InternalGetSymbolSet()->InternalGetSymbol(typeId);
        if (pTypeSymbol == nullptr || pTypeSymbol->InternalGetKind() != SvcSymbolType)
        {
            //
            // If it's orphan or constructed incorrectly, just fail.
            //
            return E_FAIL;
        }

        BaseTypeSymbol *pType = static_cast<BaseTypeSymbol *>(pTypeSymbol);

        auto typeKind = pType->InternalGetTypeKind();
        switch(typeKind)
        {
            //
            // If it's an intrinsic, make sure it's ordinal and not something like a floating point value!
            //
            case SvcSymbolTypeEnum:
            case SvcSymbolTypeIntrinsic:
            {
                SvcSymbolIntrinsicKind intrinsicKind;

                if (typeKind == SvcSymbolTypeEnum)
                {
                    EnumTypeSymbol *pEnumType = static_cast<EnumTypeSymbol *>(pType);
                    intrinsicKind = pEnumType->InternalGetEnumIntrinsicKind();
                }
                else
                {
                    BasicTypeSymbol *pBasicType = static_cast<BasicTypeSymbol *>(pType);
                    intrinsicKind = pBasicType->InternalGetIntrinsicKind();
                }

                switch(intrinsicKind)
                {
                    case SvcSymbolIntrinsicVoid:
                    case SvcSymbolIntrinsicFloat:
                    {
                        *pCanBeBitField = false;
                        *pTypeSize = 0;
                        return S_OK;
                    }

                    default:
                    {
                        *pTypeSize = pType->InternalGetTypeSize();
                        *pCanBeBitField = true;
                        return S_OK;
                    }
                }

                return E_UNEXPECTED;
            }

            //
            // If it's a typedef, chase down the underlying type and ask that.
            //
            case SvcSymbolTypeTypedef:
            {
                TypedefTypeSymbol *pTypedefType = static_cast<TypedefTypeSymbol *>(pType);
                typeId = pTypedefType->InternalGetTypedefOfTypeId();
                break;
            }

            default:
            {
                *pCanBeBitField = false;
                *pTypeSize = 0;
                return S_OK;
            }
        }
    }
}

HRESULT BaseDataSymbol::InternalSetBitFieldLength(_In_ ULONG64 bitFieldLength)
{
    HRESULT hr = S_OK;

    //
    // It's *MUCH* easier here if nothing changes.
    //
    if (bitFieldLength == m_bitFieldLength)
    {
        return S_OK;
    }

    //
    // Make *CERTAIN* that a bitfield makes sense!
    //
    if (bitFieldLength != 0)
    {
        bool canBeBitField;
        ULONG64 typeSize;
        IfFailedReturn(CanBeBitField(&canBeBitField, &typeSize));

        //
        // If the type is non-ordinal (e.g.: a UDT) or the size of the bit field is greater than the
        // size of the type, this request is gibberish.  Reject it.
        //
        if (!canBeBitField || bitFieldLength > typeSize * 8)
        {
            return E_INVALIDARG;
        }

        //
        // If there is a manually specified bitfield position and the length is changed to make the request
        // nonsensical, adjust the bitfield position manually to put it back into range.
        //
        if (m_bitFieldPosition != UdtPositionalSymbol::AutomaticAppendLayout &&
            m_bitFieldPosition > typeSize * 8 - bitFieldLength)
        {
            m_bitFieldPosition = (typeSize * 8 - bitFieldLength);
        }
    }

    m_bitFieldLength = bitFieldLength;

    hr = NotifyDependentChange();

    // 
    // Send an advisory notification upwards that everyone should flush caches.  Do not consider this
    // a failure to create the symbol if something goes wrong.  At worst, an explicit .reload will be
    // required in the debugger.
    //
    (void)InternalGetSymbolSet()->InvalidateExternalCaches();

    return hr;
}

HRESULT BaseDataSymbol::InternalSetBitFieldPosition(_In_ ULONG64 bitFieldPosition)
{
    HRESULT hr = S_OK;

    //
    // It's much easier here if nothing changes.
    //
    if (bitFieldPosition == m_bitFieldPosition)
    {
        return S_OK;
    }

    //
    // Note we will *NOT* reject a request to set this even if bitFieldLength == 0.  This allows someone
    // to change the position/field largely independently.
    //
    bool canBeBitField;
    ULONG64 typeSize;
    IfFailedReturn(CanBeBitField(&canBeBitField, &typeSize));

    //
    // If the type can't be a bitfield or the position/length combination does not make sense, reject
    // the request.
    //
    if (!canBeBitField)
    {
        return E_INVALIDARG;
    }

    if (bitFieldPosition != UdtPositionalSymbol::AutomaticAppendLayout &&
        bitFieldPosition + m_bitFieldLength > typeSize * 8)
    {
        return E_INVALIDARG;
    }

    hr = NotifyDependentChange();

    // 
    // Send an advisory notification upwards that everyone should flush caches.  Do not consider this
    // a failure to create the symbol if something goes wrong.  At worst, an explicit .reload will be
    // required in the debugger.
    //
    (void)InternalGetSymbolSet()->InvalidateExternalCaches();

    return hr;
}

HRESULT BaseDataSymbol::InternalSetSymbolValue(_In_ VARIANT const *pVal)
{
    //
    // This is *ONLY* safe because an outer layer has verified that this is an ordinal VT_[U]I[1-8].
    // We do not need to VariantCopy and the like because of this.
    //
    m_symValue = *pVal;

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

//*************************************************
// Parameters and Locals:
//

BaseScope *VariableSymbol::GetBoundScope() const 
{ 
    return static_cast<BaseScope *>(m_spBoundScope.Get());
}

HRESULT VariableSymbol::RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet,
                                               _In_ SvcSymbolKind symKind,
                                               _In_ ULONG64 parentId,
                                               _In_ ULONG64 parameterTypeId,
                                               _In_ PCWSTR pwszName)
{
    if (symKind != SvcSymbolDataParameter && symKind != SvcSymbolDataLocal)
    {
        return E_UNEXPECTED;
    }

    m_curId = 0;
    return BaseInitialize(pSymbolSet, symKind, parentId, 0ull, parameterTypeId, pwszName, nullptr, 0, 0, true);
}

HRESULT VariableSymbol::RuntimeClassInitialize(_In_ VariableSymbol *pSourceSymbol,
                                               _In_ BaseScope *pScope)
{
    auto fn = [&]()
    {
        HRESULT hr = S_OK;

        m_curId = 0;
        m_spBoundScope = pScope;

        IfFailedReturn(BaseInitialize(pSourceSymbol->InternalGetSymbolSet(),
                                      pSourceSymbol->InternalGetKind(),
                                      pSourceSymbol->InternalGetParentId(), 
                                      0ull,
                                      pSourceSymbol->InternalGetSymbolTypeId(),
                                      pSourceSymbol->InternalGetName().c_str(),
                                      nullptr,
                                      0,
                                      0,
                                      false,
                                      pSourceSymbol->InternalGetId()));

        //
        // These must be copied over such that the ordering is preserved in m_liveRangeList!
        // Normally, live range data would come from some other source that the variable symbol
        // would point at (e.g.: a DWARF DIE or some record in the PDB).  As we are synthetic,
        // the unbound VariableSymbol is the source of truth and not some external record.
        //
        auto&& liveRanges = pSourceSymbol->InternalGetLiveRanges();
        for(LiveRange const *pLiveRange : liveRanges)
        {
            auto insResult = m_liveRanges.insert( { pLiveRange->UniqueId, *pLiveRange } );
            m_liveRangeList.push_back(&insResult.first->second);
        }

        return hr;
    };
    return ConvertException(fn);
}

bool VariableSymbol::ValidateLiveRange(_In_ ULONG64 rangeOffset,
                                       _In_ ULONG64 rangeSize,
                                       _In_ ULONG64 ignoreRange)
{
    ULONG64 functionId = InternalGetParentId();
    BaseSymbol *pParentSymbol = InternalGetSymbolSet()->InternalGetSymbol(functionId);
    if (pParentSymbol == nullptr || pParentSymbol->InternalGetKind() != SvcSymbolFunction)
    {
        return false;
    }

    FunctionSymbol *pParentFunction = static_cast<FunctionSymbol *>(pParentSymbol);
    auto&& addressRanges = pParentFunction->InternalGetAddressRanges();

    //
    // Ensure that there are no areas of the "live range" which are *OUTSIDE* the bounds
    // of the function!
    //
    if (addressRanges.size() == 0)
    {
        return false;
    }

    ULONG64 functionBase = addressRanges[0].first;

    //
    // Verify that the live range is within the bounds of the function.  The range must
    // be within a single "address range" of any disjoint function because contiguous
    // ranges are reported as a single area so anything spilling outside a single range 
    // would be invalid.
    //
    ULONG64 rangeStart = functionBase + rangeOffset;
    ULONG64 rangeEnd = rangeStart + rangeSize;

    bool outsideFunction = false;
    bool found = false;

    for(auto&& functionRange : addressRanges)
    {
        ULONG64 functionStart = functionRange.first;
        ULONG64 functionEnd = functionStart + functionRange.second;

        if ((rangeStart >= functionStart && rangeStart < functionEnd) ||
            (rangeEnd >= functionStart && rangeEnd < functionEnd))
        {
            found = true;

            if (rangeStart < functionStart || rangeEnd > functionEnd)
            {
                outsideFunction = true;
            }
        }
    }

    if (outsideFunction || !found)
    {
        return false;
    }

    //
    // Ensure that this new live range does *NOT* overlap with any existing live range.
    // That would also be a failure (at least for *US* -- real symbols might have cases where
    // things might be available in one of two registers in a given basic block, etc...)
    //
    for(LiveRange const* pLiveRange : m_liveRangeList)
    {
        if (pLiveRange->UniqueId == ignoreRange)
        {
            continue;
        }

        ULONG64 travRangeStart = functionBase + pLiveRange->Offset;
        ULONG64 travRangeEnd = travRangeStart + pLiveRange->Size;

        if ((rangeStart >= travRangeStart && rangeStart < travRangeEnd) ||
            (rangeEnd >= travRangeStart && rangeEnd < travRangeEnd))
        {
            return false;
        }
    }

    return true;
}

HRESULT VariableSymbol::AddLiveRange(_In_ ULONG64 rangeOffset,
                                     _In_ ULONG64 rangeSize,
                                     _In_ SvcSymbolLocation const& varLocation,
                                     _Out_ ULONG64 *pUniqueId)
{
    auto fn = [&]()
    {
        if (!ValidateLiveRange(rangeOffset, rangeSize))
        {
            return E_INVALIDARG;
        }

        //
        // Get a unique ID (a handle) for this particular live range.
        //
        ULONG64 id = ++m_curId;

        auto insResult = m_liveRanges.insert( { id, { id, rangeOffset, rangeSize, varLocation } } );
        *pUniqueId = id;

        LiveRange * pRange = &(insResult.first->second);
        m_liveRangeList.push_back(pRange);

        // 
        // Send an advisory notification upwards that everyone should flush caches.  Do not consider
        // this a failure to set the offset if something goes wrong.  At worst, an explicit 
        // .reload will be required in the debugger.
        //
        (void)InternalGetSymbolSet()->InvalidateExternalCaches();

        return S_OK;
    };
    return ConvertException(fn);
}

HRESULT VariableSymbol::GetOffset(_Out_ ULONG64 * /*pSymbolOffset*/)
{
    //
    // We do not have any simple offset.
    //
    return E_FAIL;
}

HRESULT VariableSymbol::GetLocation(_Out_ SvcSymbolLocation *pLocation)
{
    //
    // If this isn't bound to a scope, the only time we can return a location is if there is
    // a single live range that covers the entire function.
    //
    if (!IsBoundToScope())
    {
        if (m_liveRangeList.size() != 1)
        {
            return E_FAIL;
        }

        LiveRange const *pLiveRange = m_liveRangeList[0];
        if (pLiveRange->Offset != 0)
        {
            return E_FAIL;
        }

        BaseSymbol *pParent = InternalGetSymbolSet()->InternalGetSymbol(InternalGetParentId());
        if (pParent == nullptr || pParent->InternalGetKind() != SvcSymbolFunction)
        {
            return E_FAIL;
        }

        FunctionSymbol *pFunction = static_cast<FunctionSymbol *>(pParent);
        auto&& addressRanges = pFunction->InternalGetAddressRanges();

        //
        // If the function is disjoint, a single live range cannot cover it.
        //
        if (addressRanges.size() != 1)
        {
            return E_FAIL;
        }

        std::pair<ULONG64, ULONG64> addressRange = addressRanges[0];

        if (pLiveRange->Size != addressRange.second)
        {
            return E_FAIL;
        }

        //
        // At this point, we have a guaranteed match between the live range and the entire
        // code of the function itself.  This location is *ALWAYS* valid.  It doesn't matter if 
        // we know the scope or not.
        //
        *pLocation = pLiveRange->VariableLocation;
        return S_OK;
    }
    else
    {
        ULONG64 srelOffset = GetBoundScope()->InternalGetFunctionOffset();
        FunctionSymbol *pFunction = GetBoundScope()->InternalGetFunction();

        //
        // Sanity check that the scope we are bound to is within our parent function!
        //
        if (pFunction->InternalGetId() != InternalGetParentId())
        {
            return E_UNEXPECTED;
        }

        LiveRange const* pLiveRange = GetLiveRangeByOffset(srelOffset);
        if (pLiveRange == nullptr)
        {
            //
            // It is not alive at this particular location.
            //
            pLocation->Kind = SvcSymbolLocationNone;
            return S_OK;
        }

        *pLocation = pLiveRange->VariableLocation;
        return S_OK;
    }
}

bool VariableSymbol::InternalSetLiveRangeOffset(_In_ ULONG64 id, _In_ ULONG64 offset)
{
    LiveRange *pLiveRange = GetLiveRange(id);
    if (pLiveRange == nullptr ||
        !ValidateLiveRange(offset, pLiveRange->Size, id))
    {
        return false;
    }

    pLiveRange->Offset = offset;

    // 
    // Send an advisory notification upwards that everyone should flush caches.  Do not consider this
    // a failure to set the offset if something goes wrong.  At worst, an explicit .reload will be
    // required in the debugger.
    //
    (void)InternalGetSymbolSet()->InvalidateExternalCaches();
    return true;
}

bool VariableSymbol::InternalSetLiveRangeSize(_In_ ULONG64 id, _In_ ULONG64 size)
{
    LiveRange *pLiveRange = GetLiveRange(id);
    if (pLiveRange == nullptr ||
        !ValidateLiveRange(pLiveRange->Offset, size, id))
    {
        return false;
    }

    pLiveRange->Size = size;

    // 
    // Send an advisory notification upwards that everyone should flush caches.  Do not consider this
    // a failure to set the offset if something goes wrong.  At worst, an explicit .reload will be
    // required in the debugger.
    //
    (void)InternalGetSymbolSet()->InvalidateExternalCaches();
    return true;
}

bool VariableSymbol::InternalSetLiveRangeLocation(_In_ ULONG64 id, _In_ SvcSymbolLocation const& location)
{
    LiveRange *pLiveRange = GetLiveRange(id);
    if (pLiveRange == nullptr)
    {
        return false;
    }

    pLiveRange->VariableLocation = location;

    // 
    // Send an advisory notification upwards that everyone should flush caches.  Do not consider this
    // a failure to set the offset if something goes wrong.  At worst, an explicit .reload will be
    // required in the debugger.
    //
    (void)InternalGetSymbolSet()->InvalidateExternalCaches();
    return true;
}

bool VariableSymbol::InternalDeleteLiveRange(_In_ ULONG64 id)
{
    auto fn = [&]()
    {
        bool found = false;

        auto it = m_liveRanges.find(id);

        for (size_t i = 0; i < m_liveRangeList.size(); ++i)
        {
            if (m_liveRangeList[i]->UniqueId == id)
            {
                found = true;
                m_liveRangeList.erase(m_liveRangeList.begin() + i);
                break;
            }
        }

        if (it == m_liveRanges.end() || !found)
        {
            return E_FAIL;
        }

        m_liveRanges.erase(it);
        return S_OK;
    };
    if (FAILED(ConvertException(fn)))
    {
        return false;
    }

    // 
    // Send an advisory notification upwards that everyone should flush caches.  Do not consider this
    // a failure to set the offset if something goes wrong.  At worst, an explicit .reload will be
    // required in the debugger.
    //
    (void)InternalGetSymbolSet()->InvalidateExternalCaches();

    return true;
}

void VariableSymbol::InternalDeleteAllLiveRanges()
{
    auto fn = [&]()
    {
        m_liveRangeList.clear();
        m_liveRanges.clear();
        return S_OK;
    };
    (void)ConvertException(fn);
}

HRESULT VariableSymbol::MoveToBefore(_In_ ULONG64 position)
{
    if (InternalGetKind() != SvcSymbolDataParameter)
    {
        return E_UNEXPECTED;
    }

    BaseSymbol *pParentSymbol = InternalGetSymbolSet()->InternalGetSymbol(m_parentId);
    if (pParentSymbol == nullptr)
    {
        return E_UNEXPECTED;
    }

    return pParentSymbol->MoveChildBefore(InternalGetId(), position, InternalGetKind());
}


} // SymbolBuilder
} // Services
} // TargetComposition
} // Debugger
