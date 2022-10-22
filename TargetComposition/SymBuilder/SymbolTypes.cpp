//**************************************************************************
//
// SymbolTypes.cpp
//
// The implementation of types within a "symbol set".  A "symbol set" is an
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
// UDT Positional Symbols (Fields / Base Classes / Etc...)
//

HRESULT UdtPositionalSymbol::BaseInitialize(_In_ SymbolSet *pSymbolSet,
                                            _In_ SvcSymbolKind symKind,
                                            _In_ ULONG64 owningTypeId,
                                            _In_ ULONG64 symOffset,
                                            _In_ ULONG64 symTypeId,
                                            _In_opt_ PCWSTR pwszName)
{
    //
    // The base data symbol doesn't have a distinction between offset and actual offset.  Only we do.  Mark
    // the actual offset the same as the offset.  Any triggering of type layout will change this if such
    // is marked as automatic.
    //
    m_symOffsetActual = symOffset;
    return BaseDataSymbol::BaseInitialize(pSymbolSet, symKind, owningTypeId, symOffset, symTypeId, pwszName, nullptr);
}

HRESULT UdtPositionalSymbol::BaseInitialize(_In_ SymbolSet *pSymbolSet,
                                            _In_ ULONG64 owningTypeId,
                                            _In_ VARIANT *pValue,
                                            _In_ ULONG64 symTypeId,
                                            _In_ PCWSTR pwszName)
{
    //
    // The base data symbol doesn't have a distinction between offset and actual offset.  Only we do.  Mark
    // the actual offset the same as the offset.  Any triggering of type layout will change this if such
    // is marked as automatic.
    //
    m_symOffsetActual = m_symOffset;
    return BaseDataSymbol::BaseInitialize(pSymbolSet, SvcSymbolField, owningTypeId, pValue, symTypeId, pwszName, nullptr);
}

HRESULT UdtPositionalSymbol::MoveToBefore(_In_ ULONG64 position)
{
    BaseSymbol *pParentSymbol = InternalGetSymbolSet()->InternalGetSymbol(m_parentId);
    if (pParentSymbol == nullptr)
    {
        return E_UNEXPECTED;
    }

    return pParentSymbol->MoveChildBefore(InternalGetId(), position, InternalGetKind());
}

//*************************************************
// UDT Symbols:
//

HRESULT UdtTypeSymbol::LayoutType()
{
    ULONG64 typeSize = 0; 
    ULONG64 curOffset = 0;
    ULONG64 maxAlignment = 1;

    SvcSymbolKind passKinds[] = { SvcSymbolBaseClass, SvcSymbolField };

    //
    // Walk through all our children (mostly looking at fields) and figure out where each field goes
    // in the layout given alignment, packing, and sizes.
    //
    // We need to make this pass several times because things like base classes must come *BEFORE* fields
    // and the like.
    //
    auto&& children = InternalGetChildren();
    for (size_t pass = 0; pass < ARRAYSIZE(passKinds); ++pass)
    {
        SvcSymbolKind passKind = passKinds[pass];

        for (size_t i = 0; i < children.size(); ++i)
        {
            BaseSymbol *pBaseSymbol = InternalGetSymbolSet()->InternalGetSymbol(children[i]);
            if (pBaseSymbol == nullptr)
            {
                return E_UNEXPECTED;
            }

            if (pBaseSymbol->InternalGetKind() != passKind)
            {
                continue;
            }

            UdtPositionalSymbol *pPosSymbol = static_cast<UdtPositionalSymbol *>(pBaseSymbol);

            //
            // Find the type of the field and gather basic information about size/alignment to see
            // if we need to add requisite padding (assuming this is an auto-layout field).  If the field offset
            // was manually specified, it goes there REGARDLESS of what the alignment says.
            //
            ULONG64 symTypeId = pPosSymbol->InternalGetSymbolTypeId();
            BaseSymbol *pSymbolTypeBaseSymbol = InternalGetSymbolSet()->InternalGetSymbol(symTypeId);
            if (pSymbolTypeBaseSymbol == nullptr || pSymbolTypeBaseSymbol->InternalGetKind() != SvcSymbolType)
            {
                return E_UNEXPECTED;
            }

            BaseTypeSymbol *pSymbolTypeBase = static_cast<BaseTypeSymbol *>(pSymbolTypeBaseSymbol);

            ULONG64 symTypeSize = pSymbolTypeBase->InternalGetTypeSize();
            ULONG64 symTypeAlign = pSymbolTypeBase->InternalGetTypeAlignment();

            if (symTypeAlign > maxAlignment)
            {
                maxAlignment = symTypeAlign;
            }

            ULONG64 symOffset = pPosSymbol->InternalGetSymbolOffset();
            bool autoLayoutField = (symOffset == UdtPositionalSymbol::AutomaticAppendLayout);

            if (autoLayoutField)
            {
                symOffset = curOffset;
                if (symTypeAlign != 1)
                {
                    symOffset = ((symOffset + (symTypeAlign - 1)) / symTypeAlign) * symTypeAlign;
                }
                pPosSymbol->InternalSetComputedSymbolOffset(symOffset);
            }

            curOffset = symOffset + symTypeSize;
            if (typeSize < curOffset)
            {
                typeSize = curOffset;
            }
        }
    }

    m_typeAlignment = maxAlignment;
    if (maxAlignment != 1)
    {
        typeSize = ((typeSize + (maxAlignment - 1)) / maxAlignment) * maxAlignment;
    }
    m_typeSize = typeSize;

    return S_OK;
}

//*************************************************
// Pointer Symbols:
//

HRESULT PointerTypeSymbol::GetBaseType(_Out_ ISvcSymbol **ppBaseType)
{
    HRESULT hr = S_OK;
    *ppBaseType = nullptr;

    BaseSymbol *pPointerToSymbol = InternalGetSymbolSet()->InternalGetSymbol(m_pointerToId);
    if (pPointerToSymbol == nullptr)
    {
        return E_FAIL;
    }

    ComPtr<ISvcSymbol> spPointerToSymbol = pPointerToSymbol;
    *ppBaseType = spPointerToSymbol.Detach();
    return hr;
}

HRESULT PointerTypeSymbol::RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet,
                                                  _In_ ULONG64 pointerToId,
                                                  _In_ SvcSymbolPointerKind pointerKind)
{
    //
    // We cannot let a C++ exception escape.
    //
    auto fn = [&]()
    {
        HRESULT hr = S_OK;

        ISvcMachineArchitecture *pArchInfo = pSymbolSet->GetArchInfo();
        if (pArchInfo == nullptr)
        {
            return E_UNEXPECTED;
        }

        BaseSymbol *pPointerToSymbol = pSymbolSet->InternalGetSymbol(pointerToId);
        if (pPointerToSymbol == nullptr || pPointerToSymbol->InternalGetKind() != SvcSymbolType)
        {
            return E_INVALIDARG;
        }

        std::wstring const& name = pPointerToSymbol->InternalGetName();
        std::wstring const& qualifiedName = pPointerToSymbol->InternalGetQualifiedName();

        std::wstring ptrName;
        std::wstring ptrQualifiedName;

        if (!name.empty())
        {
            ptrName = name;
            AppendPtrChar(ptrName, pointerKind);
        }

        if (!qualifiedName.empty())
        {
            ptrQualifiedName = qualifiedName;
            AppendPtrChar(ptrQualifiedName, pointerKind);
        }

        IfFailedReturn(BaseTypeSymbol::BaseInitialize(pSymbolSet, 
                                                      SvcSymbolType, 
                                                      SvcSymbolTypePointer,
                                                      0,
                                                      ptrName.empty() ? nullptr : ptrName.c_str(),
                                                      ptrQualifiedName.empty() ? nullptr : ptrQualifiedName.c_str()));

        m_pointerKind = pointerKind;
        m_pointerToId = pointerToId;
        
        m_typeSize = pArchInfo->GetBitness() / 8;
        m_typeAlignment = pArchInfo->GetBitness() / 8;

        return S_OK;
    };
    return ConvertException(fn);
}

//*************************************************
// Array Symbols:
//

HRESULT ArrayTypeSymbol::GetBaseType(_Out_ ISvcSymbol **ppBaseType)
{
    HRESULT hr = S_OK;
    *ppBaseType = nullptr;

    BaseSymbol *pArrayOfSymbol = InternalGetSymbolSet()->InternalGetSymbol(m_arrayOfTypeId);
    if (pArrayOfSymbol == nullptr)
    {
        return E_FAIL;
    }

    ComPtr<ISvcSymbol> spArrayOfSymbol = pArrayOfSymbol;
    *ppBaseType = spArrayOfSymbol.Detach();
    return hr;
}

HRESULT ArrayTypeSymbol::GetArrayDimensions(_In_ ULONG64 dimensions,
                                            _Out_writes_(dimensions) SvcSymbolArrayDimension *pDimensions)
{
    if (dimensions != 1)
    {
        return E_INVALIDARG;
    }

    pDimensions[0].DimensionFlags = 0;
    pDimensions[0].LowerBound = 0;
    pDimensions[0].Length = m_arrayDim;
    pDimensions[0].Stride = m_baseTypeSize;
    return S_OK;
}

HRESULT ArrayTypeSymbol::RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet,
                                                _In_ ULONG64 arrayOfId,
                                                _In_ ULONG64 arrayDim)
{
    //
    // We cannot let a C++ exception escape.
    //
    auto fn = [&]()
    {
        HRESULT hr = S_OK;

        wchar_t arBuf[64];
        swprintf_s(arBuf, ARRAYSIZE(arBuf), L"[%I64d]", arrayDim);

        BaseSymbol *pArrayOfSymbol = pSymbolSet->InternalGetSymbol(arrayOfId);
        if (pArrayOfSymbol == nullptr || pArrayOfSymbol->InternalGetKind() != SvcSymbolType)
        {
            return E_INVALIDARG;
        }

        BaseTypeSymbol *pArrayOfType = static_cast<BaseTypeSymbol *>(pArrayOfSymbol);
        ULONG64 arrayOfTypeSize = pArrayOfType->InternalGetTypeSize();
        ULONG64 arrayOfTypeAlign = pArrayOfType->InternalGetTypeAlignment();

        std::wstring const& name = pArrayOfSymbol->InternalGetName();
        std::wstring const& qualifiedName = pArrayOfSymbol->InternalGetQualifiedName();

        std::wstring arrayName;
        std::wstring arrayQualifiedName;

        if (!name.empty())
        {
            arrayName = name;
            arrayName += arBuf;
        }

        if (!qualifiedName.empty())
        {
            arrayQualifiedName = qualifiedName;
            arrayQualifiedName += arBuf;
        }

        IfFailedReturn(BaseTypeSymbol::BaseInitialize(pSymbolSet, 
                                                      SvcSymbolType, 
                                                      SvcSymbolTypeArray,
                                                      0,
                                                      arrayName.empty() ? nullptr : arrayName.c_str(),
                                                      arrayQualifiedName.empty() ? nullptr : arrayQualifiedName.c_str()));

        m_arrayDim = arrayDim;
        m_arrayOfTypeId = arrayOfId;
        m_baseTypeSize = arrayOfTypeSize;
        
        m_typeSize = (m_baseTypeSize * m_arrayDim);
        m_typeAlignment = arrayOfTypeAlign;

        //
        // Add a dependency notification between the type of this array and us owning type.  That way,
        // if the layout of the underlying type changes, we can recompute our own layout (and do this all the way
        // up any dependency tree).
        //
        IfFailedReturn(pArrayOfType->AddDependentNotify(InternalGetId()));

        return S_OK;
    };
    return ConvertException(fn);
}

HRESULT ArrayTypeSymbol::NotifyDependentChange()
{
    BaseSymbol *pArrayOfSymbol = InternalGetSymbolSet()->InternalGetSymbol(m_arrayOfTypeId);
    if (pArrayOfSymbol == nullptr || pArrayOfSymbol->InternalGetKind() != SvcSymbolType)
    {
        return E_INVALIDARG;
    }

    BaseTypeSymbol *pArrayOfType = static_cast<BaseTypeSymbol *>(pArrayOfSymbol);

    m_baseTypeSize = pArrayOfType->InternalGetTypeSize();
    m_typeAlignment = pArrayOfType->InternalGetTypeAlignment();
    m_typeSize = (m_baseTypeSize * m_arrayDim);

    return BaseSymbol::NotifyDependentChange();
}

HRESULT ArrayTypeSymbol::Delete()
{
    HRESULT hr = S_OK;

    BaseSymbol *pArrayOfType = InternalGetSymbolSet()->InternalGetSymbol(m_arrayOfTypeId);
    if (pArrayOfType != nullptr)
    {
        //
        // Remove a dependency notification between the type of this array and us.  We are going away and no
        // longer need the notification.
        //
        IfFailedReturn(pArrayOfType->RemoveDependentNotify(InternalGetId()));
    }

    return BaseSymbol::Delete();
}

//*************************************************
// Typedef Symbols:
//

HRESULT TypedefTypeSymbol::GetBaseType(_Out_ ISvcSymbol **ppBaseType)
{
    HRESULT hr = S_OK;
    *ppBaseType = nullptr;

    BaseSymbol *pTypedefOfSymbol = InternalGetSymbolSet()->InternalGetSymbol(m_typedefOfTypeId);
    if (pTypedefOfSymbol == nullptr)
    {
        return E_FAIL;
    }

    ComPtr<ISvcSymbol> spTypedefOfSymbol = pTypedefOfSymbol;
    *ppBaseType = spTypedefOfSymbol.Detach();
    return hr;
}

HRESULT TypedefTypeSymbol::RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet,
                                                  _In_ ULONG64 typedefOfId,
                                                  _In_ ULONG64 parentId,
                                                  _In_ PCWSTR pwszName,
                                                  _In_opt_ PCWSTR pwszQualifiedName)
{
    HRESULT hr = S_OK;

    BaseSymbol *pTypedefOfSymbol = pSymbolSet->InternalGetSymbol(typedefOfId);
    if (pTypedefOfSymbol == nullptr || pTypedefOfSymbol->InternalGetKind() != SvcSymbolType)
    {
        return E_INVALIDARG;
    }

    BaseTypeSymbol *pTypedefOfType = static_cast<BaseTypeSymbol *>(pTypedefOfSymbol);

    IfFailedReturn(BaseTypeSymbol::BaseInitialize(pSymbolSet, 
                                                  SvcSymbolType, 
                                                  SvcSymbolTypeTypedef,
                                                  parentId,
                                                  pwszName,
                                                  pwszQualifiedName));

    m_typedefOfTypeId = typedefOfId;

    m_typeSize = pTypedefOfType->InternalGetTypeSize();
    m_typeAlignment = pTypedefOfType->InternalGetTypeAlignment();
    return hr;
}

//*************************************************
// Enum Symbols:
//

HRESULT EnumTypeSymbol::GetBaseType(_Out_ ISvcSymbol **ppBaseType)
{
    HRESULT hr = S_OK;
    *ppBaseType = nullptr;

    BaseSymbol *pEnumBaseType = InternalGetSymbolSet()->InternalGetSymbol(m_enumBasicTypeId);
    if (pEnumBaseType == nullptr)
    {
        return E_FAIL;
    }

    ComPtr<ISvcSymbol> spEnumBaseType = pEnumBaseType;
    *ppBaseType = spEnumBaseType.Detach();
    return hr;
}

HRESULT EnumTypeSymbol::RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet,
                                               _In_ ULONG64 enumBasicTypeId,
                                               _In_ ULONG64 parentId,
                                               _In_ PCWSTR pwszName,
                                               _In_opt_ PCWSTR pwszQualifiedName)
{
    HRESULT hr = S_OK;

    //
    // The base type of the enum *MUST* be a basic type which is ordinal.  Validate this.
    //
    BaseSymbol *pEnumBasicTypeSymbol = pSymbolSet->InternalGetSymbol(enumBasicTypeId);
    if (pEnumBasicTypeSymbol == nullptr || pEnumBasicTypeSymbol->InternalGetKind() != SvcSymbolType)
    {
        return E_INVALIDARG;
    }

    BaseTypeSymbol *pEnumType = static_cast<BaseTypeSymbol *>(pEnumBasicTypeSymbol);
    if (pEnumType->InternalGetTypeKind() != SvcSymbolTypeIntrinsic)
    {
        return E_INVALIDARG;
    }

    //
    // Keep track of how values for this enum must pack based on the underlying basic type.
    //
    VARTYPE enumPacking = VT_EMPTY;
    bool isSigned = false;

    BasicTypeSymbol *pEnumBasicType = static_cast<BasicTypeSymbol *>(pEnumType);
    m_enumIntrinsicKind = pEnumBasicType->InternalGetIntrinsicKind();
    switch(m_enumIntrinsicKind)
    {
        case SvcSymbolIntrinsicBool:
        {
            enumPacking = VT_BOOL;
            break;
        }

        case SvcSymbolIntrinsicChar:
        case SvcSymbolIntrinsicInt:
        case SvcSymbolIntrinsicLong:
        {
            isSigned = true;
            break;
        }

        case SvcSymbolIntrinsicWChar:
        case SvcSymbolIntrinsicUInt:
        case SvcSymbolIntrinsicULong:
        {
            isSigned = false;
            break;
        }

        default:
            return E_INVALIDARG;
    }

    //
    // If we haven't figured out a packing, it's a default ordinal and go with that path based on
    // the underlying size of the type.
    //
    if (enumPacking == VT_EMPTY)
    {
        switch(pEnumBasicType->InternalGetTypeSize())
        {
            case 1:
                enumPacking = (VARTYPE)(isSigned ? VT_I1 : VT_UI1);
                break;
            
            case 2:
                enumPacking = (VARTYPE)(isSigned ? VT_I2 : VT_UI2);
                break;

            case 4:
                enumPacking = (VARTYPE)(isSigned ? VT_I4 : VT_UI4);
                break;

            case 8:
                enumPacking = (VARTYPE)(isSigned ? VT_I8 : VT_UI8);
                break;

            default:
                return E_INVALIDARG;
        }
    }

    IfFailedReturn(BaseTypeSymbol::BaseInitialize(pSymbolSet, 
                                                  SvcSymbolType, 
                                                  SvcSymbolTypeEnum,
                                                  parentId,
                                                  pwszName,
                                                  pwszQualifiedName));

    m_enumBasicTypeId = enumBasicTypeId;
    m_enumPacking = enumPacking;

    m_typeSize = pEnumType->InternalGetTypeSize();
    m_typeAlignment = pEnumType->InternalGetTypeAlignment();

    return hr;
}

HRESULT EnumTypeSymbol::LayoutEnum()
{
    VARIANT vtVal;

    memset(&vtVal, 0, sizeof(vtVal));
    vtVal.vt = m_enumPacking;

    //
    // Walk through all our children (looking at enumerants) and figure out the actual values for any enumerant
    // which is an "automatic increase" over the previous enumerant.
    //
    auto&& children = InternalGetChildren();
    bool foundFirst = false;
    for (size_t i = 0; i < children.size(); ++i)
    {
        BaseSymbol *pBaseSymbol = InternalGetSymbolSet()->InternalGetSymbol(children[i]);
        if (pBaseSymbol == nullptr)
        {
            return E_UNEXPECTED;
        }

        if (pBaseSymbol->InternalGetKind() != SvcSymbolField)
        {
            continue;
        }

        UdtPositionalSymbol *pPosSymbol = static_cast<UdtPositionalSymbol *>(pBaseSymbol);

        if (pPosSymbol->InternalIsConstantValue())
        {
            if (pPosSymbol->InternalIsIncreasingConstant())
            {
                if (foundFirst)
                {
                    switch(vtVal.vt)
                    {
                        case VT_I1:
                            vtVal.cVal++;
                            break;
                        case VT_I2:
                            vtVal.iVal++;
                            break;
                        case VT_I4:
                            vtVal.lVal++;
                            break;
                        case VT_I8:
                            vtVal.llVal++;
                            break;
                        case VT_UI1:
                            vtVal.bVal++;
                            break;
                        case VT_UI2:
                            vtVal.uiVal++;
                            break;
                        case VT_UI4:
                            vtVal.ulVal++;
                            break;
                        case VT_UI8:
                            vtVal.ullVal++;
                            break;
                        case VT_BOOL:
                            if (vtVal.boolVal == VARIANT_FALSE)
                            {
                                vtVal.boolVal = VARIANT_TRUE;
                            }
                            break;
                        default:
                            break;
                    }
                }

                pPosSymbol->InternalSetComputedValue(vtVal);
            }
            else
            {
                vtVal = pPosSymbol->InternalGetSymbolValue();
            }

            foundFirst = true;
        }
    }

    return S_OK;
}

//*************************************************
// Function Types:
//

HRESULT FunctionTypeSymbol::RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet,
                                                   _In_ ULONG64 returnTypeId,
                                                   _In_ ULONG64 paramCount,
                                                   _In_reads_(paramCount) ULONG64 *pParamTypes)
{
    auto fn = [&]()
    {
        HRESULT hr = S_OK;

        if (paramCount > std::numeric_limits<size_t>::max())
        {
            return E_INVALIDARG;
        }

        m_returnType = returnTypeId;
        for (ULONG64 i = 0; i < paramCount; ++i)
        {
            m_paramTypes.push_back(pParamTypes[static_cast<size_t>(i)]);
        }

        IfFailedReturn(BaseSymbol::BaseInitialize(pSymbolSet, SvcSymbolType, 0, nullptr, nullptr));

        return hr;
    };
    return ConvertException(fn);
}

HRESULT FunctionTypeSymbol::GetFunctionReturnType(_COM_Outptr_ ISvcSymbol **ppReturnType)
{
    HRESULT hr = S_OK;
    *ppReturnType = nullptr;

    BaseSymbol *pReturnTypeSymbol = InternalGetSymbolSet()->InternalGetSymbol(m_returnType);
    if (pReturnTypeSymbol == nullptr || pReturnTypeSymbol->InternalGetKind() != SvcSymbolType)
    {
        return E_INVALIDARG;
    }

    ComPtr<ISvcSymbol> spReturnType = pReturnTypeSymbol;
    *ppReturnType = spReturnType.Detach();
    return hr;
}

HRESULT FunctionTypeSymbol::GetFunctionParameterTypeAt(_In_ ULONG64 i,
                                                       _COM_Outptr_ ISvcSymbol **ppParameterType)
{
    HRESULT hr = S_OK;
    *ppParameterType = nullptr;

    if (i >= m_paramTypes.size())
    {
        return E_INVALIDARG;
    }

    ULONG64 paramType = m_paramTypes[static_cast<size_t>(i)];

    BaseSymbol *pParamTypeSymbol = InternalGetSymbolSet()->InternalGetSymbol(paramType);
    if (pParamTypeSymbol == nullptr || pParamTypeSymbol->InternalGetKind() != SvcSymbolType)
    {
        return E_UNEXPECTED;
    }

    ComPtr<ISvcSymbol> spParameterType = pParamTypeSymbol;
    *ppParameterType = spParameterType.Detach();
    return hr;
}

} // SymbolBuilder
} // Services
} // TargetComposition
} // Debugger
