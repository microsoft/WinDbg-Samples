//**************************************************************************
//
// SymbolTypes.h
//
// The header for our implementation of types within a "symbol set".  A "symbol set" is an
// abstraction for the available symbols for a given module.  It is a set of
// "stacked" interfaces which implements progressively more functionality depending
// on the complexity of the symbol implementation.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#ifndef __SYMBOLTYPES_H__
#define __SYMBOLTYPES_H__

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace SymbolBuilder
{

// BaseTypeSymbol:
//
// Our base class for all type symbols
//
class BaseTypeSymbol : 
    public Microsoft::WRL::Implements<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        BaseSymbol,
        ISvcSymbolType
        >
{
public:

    //*************************************************
    // ISvcSymbolType:
    //

    // GetTypeKind():
    //
    // Gets the kind of type symbol that this is (e.g.: base type, struct, array, etc...)
    //
    IFACEMETHOD(GetTypeKind)(_Out_ SvcSymbolTypeKind *pTypeKind)
    {
        *pTypeKind = m_typeKind;
        return S_OK;
    }

    // GetSize():
    //
    // Gets the overall size of the type as laid out in memory.
    //
    IFACEMETHOD(GetSize)(_Out_ ULONG64 *pSize)
    {
        *pSize = m_typeSize;
        return S_OK;
    }

    // GetBaseType():
    //
    // If the type is a derivation of another single type (e.g.: as "MyStruct *" is derived from "MyStruct"),
    // this returns the base type of the derivation.  For pointers, this would return the type pointed to.
    // For arrays, this would return what the array is an array of.  If the type is not such a derivative
    // type, an error is returned.
    //
    // Note that this method has nothing to do with C++ base classes.  Such are symbols which can be enumerated
    // from the derived class.
    //
    IFACEMETHOD(GetBaseType)(_Out_ ISvcSymbol **ppBaseType)
    {
        //
        // By default, we have no "base type."  A derived class may need to override this.
        //
        *ppBaseType = nullptr;
        return E_NOTIMPL;
    }

    // GetUnmodifiedType():
    //
    // If the type is a qualified form (const/volatile/etc...) of another type, this returns a type symbol
    // with all qualifiers stripped.
    //
    IFACEMETHOD(GetUnmodifiedType)(_Out_ ISvcSymbol **ppUnmodifiedType)
    {
        //
        // By default, we have no "unmodified type."  A derived class may need to override this.
        //
        *ppUnmodifiedType = nullptr;
        return E_NOTIMPL;
    }

    // GetIntrinsicType():
    //
    // If the type kind as reported by GetTypeKind is an intrinsic, this returns more information about
    // the particular kind of intrinsic.
    //
    IFACEMETHOD(GetIntrinsicType)(_Out_opt_ SvcSymbolIntrinsicKind * /*pKind*/,
                                  _Out_opt_ ULONG * /*pPackingSize*/)
    {
        //
        // By default, we have no "intrinsic type" information.  A derived class may need to override this.
        //
        return E_NOTIMPL;
    }

    // GetPointerKind():
    //
    // Returns what kind of pointer the type is (e.g.: a standard pointer, a pointer to member,
    // a reference, an r-value reference, etc...
    //
    IFACEMETHOD(GetPointerKind)(_Out_ SvcSymbolPointerKind * /*pKind*/)
    {
        //
        // By default, we have no "pointer kind."  A derived class may need to override this.
        //
        return E_NOTIMPL;
    }

    // GetMemberType():
    //
    // If the pointer is a pointer-to-class-member, this returns the type of such class.
    //
    IFACEMETHOD(GetMemberType)(_COM_Outptr_ ISvcSymbolType **ppMemberType)
    {
        //
        // By default, we have no "member type."  A derived class may need to override this.
        //
        *ppMemberType = nullptr;
        return E_NOTIMPL;
    }

    // GetArrayDimensionality():
    //
    // Returns the dimensionality of the array.  There is no guarantee that every array type representable by
    // these interfaces is a standard zero-based one dimensional array as is standard in C.
    //
    IFACEMETHOD(GetArrayDimensionality)(_Out_ ULONG64 * /*pArrayDimensionality*/)
    {
        //
        // By default, we have no "array dimensionality."  A derived class may need to override this.
        //
        return E_NOTIMPL;
    }

    // GetArrayDimensions():
    //
    // Fills in information about each dimension of the array including its lower bound, length, and
    // stride.
    //
    IFACEMETHOD(GetArrayDimensions)(_In_ ULONG64 dimensions,
                                    _Out_writes_(dimensions) SvcSymbolArrayDimension * /*pDimensions*/)
    {
        //
        // By default, we have no "array dimensions."  A derived class may need to override this.
        //
        UNREFERENCED_PARAMETER(dimensions);
        return E_NOTIMPL;
    }

    // GetArrayHeaderSize():
    //
    // Gets the size of any header of the array (this is the offset of the first element of the array as
    // described by the dimensions).
    //
    // This should *ALWAYS* return 0 for a C style array.
    //
    IFACEMETHOD(GetArrayHeaderSize)(_Out_ ULONG64 * /*pArrayHeaderSize*/)
    {
        //
        // By default, we have no "array header size."  A derived class may need to override this.
        //
        return E_NOTIMPL;
    }

    // GetFunctionReturnType():
    //
    // Returns the return type of a function.  Even non-value returning functions (e.g.: void) should return
    // a type representing this.
    //
    IFACEMETHOD(GetFunctionReturnType)(_COM_Outptr_ ISvcSymbol **ppReturnType)
    {
        //
        // By default, we have no "function return type."  A derived class may need to override this.
        //
        *ppReturnType = nullptr;
        return E_NOTIMPL;
    }

    // GetFunctionParameterTypeCount():
    //
    // Returns the number of parameters that the function takes.
    //
    IFACEMETHOD(GetFunctionParameterTypeCount)(_Out_ ULONG64 * /*pCount*/)
    {
        //
        // By default, we have no "function parameter type count."  A derived class may need to override this.
        //
        return E_NOTIMPL;
    }

    // GetFunctionParameterTypeAt():
    //
    // Returns the type of the "i"-th argument to the function as a new ISvcSymbol
    //
    IFACEMETHOD(GetFunctionParameterTypeAt)(_In_ ULONG64 /*i*/,
                                            _COM_Outptr_ ISvcSymbol **ppParameterType)
    {
        //
        // By default, we have no "function parameter types."  A derived class may need to override this.
        //
        *ppParameterType = nullptr;
        return E_NOTIMPL;
    }

    //*************************************************
    // Internal APIs:
    //

    HRESULT BaseInitialize(_In_ SymbolSet *pSymbolSet,
                           _In_ SvcSymbolKind kind,
                           _In_ SvcSymbolTypeKind typeKind,
                           _In_ ULONG64 parentId,
                           _In_opt_ PCWSTR pwszSymbolName = nullptr,
                           _In_opt_ PCWSTR pwszQualifiedName = nullptr,
                           _In_ ULONG64 reservedId = 0)
    {
        HRESULT hr = S_OK;
        IfFailedReturn(BaseSymbol::BaseInitialize(pSymbolSet, kind, parentId, pwszSymbolName, pwszQualifiedName, true, reservedId));
        m_typeKind = typeKind;
        m_typeSize = 0;
        m_typeAlignment = 1;
        return hr;
    }

    //*************************************************
    // Internal Accessors():
    //

    SvcSymbolTypeKind InternalGetTypeKind() const { return m_typeKind; }
    ULONG64 InternalGetTypeSize() const { return m_typeSize; }
    ULONG64 InternalGetTypeAlignment() const { return m_typeAlignment; }

protected:

    SvcSymbolTypeKind m_typeKind;
    ULONG64 m_typeSize;
    ULONG64 m_typeAlignment;
};

// BasicTypeSymbol:
//
// A type symbol which represents some basic type (e.g.: int, float, etc...)
//
class BasicTypeSymbol :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        BaseTypeSymbol
        >
{
public:

    //*************************************************
    // ISvcSymbolType:
    //

    // GetIntrinsicType():
    //
    // If the type kind as reported by GetTypeKind is an intrinsic, this returns more information about
    // the particular kind of intrinsic.
    //
    IFACEMETHOD(GetIntrinsicType)(_Out_opt_ SvcSymbolIntrinsicKind *pKind,
                                  _Out_opt_ ULONG *pPackingSize)
    {
        *pKind = m_intrinsicKind;
        *pPackingSize = static_cast<ULONG>(m_typeSize);
        return S_OK;
    }

    //*************************************************
    // Internal APIs:
    //

    HRESULT RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet,
                                   _In_ SvcSymbolIntrinsicKind intrinsicKind,
                                   _In_ ULONG packingSize,
                                   _In_ PCWSTR pwszName)
    {
        HRESULT hr = S_OK;
        IfFailedReturn(BaseInitialize(pSymbolSet, SvcSymbolType, SvcSymbolTypeIntrinsic, 0, pwszName));
        m_intrinsicKind = intrinsicKind;
        m_typeSize = packingSize;
        m_typeAlignment = packingSize;
        return S_OK;
    }

    //*************************************************
    // Internal Accessors:
    //

    SvcSymbolIntrinsicKind InternalGetIntrinsicKind() const { return m_intrinsicKind; }

private:

    SvcSymbolIntrinsicKind m_intrinsicKind;

};

// UdtTypeSymbol:
//
// A type symbol which represents some user defined type (e.g.: struct, class, etc...)
//
class UdtTypeSymbol :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        BaseTypeSymbol
        >
{
public:

    //*************************************************
    // Internal APIs:
    //

    virtual HRESULT NotifyDependentChange()
    {
        HRESULT hr = LayoutType();
        if (SUCCEEDED(hr))
        {
            hr = BaseSymbol::NotifyDependentChange();
        }
        return hr;
    }

    HRESULT RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet,
                                   _In_ ULONG64 parentId,
                                   _In_ PCWSTR pwszName,
                                   _In_opt_ PCWSTR pwszQualifiedName)
    {
        return BaseInitialize(pSymbolSet, SvcSymbolType, SvcSymbolTypeUDT, parentId, pwszName, pwszQualifiedName);
    }

    // LayoutType():
    //
    // Performs a type layout.  This computes everything necessary about a type from its field layout including
    // the offsets of fields, the size of the type, and any alignment padding necessary.
    //
    HRESULT LayoutType();

private:

};

// PointerTypeSymbol:
//
// A symbol which represents a pointer to some other type
//
class PointerTypeSymbol :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        BaseTypeSymbol
        >
{
public:

    //*************************************************
    // ISvcSymbolType:
    //

    // GetPointerKind():
    //
    // Returns what kind of pointer the type is (e.g.: a standard pointer, a pointer to member,
    // a reference, an r-value reference, etc...
    //
    IFACEMETHOD(GetPointerKind)(_Out_ SvcSymbolPointerKind *pKind)
    {
        *pKind = m_pointerKind;
        return S_OK;
    }

    // GetBaseType():
    //
    // If the type is a derivation of another single type (e.g.: as "MyStruct *" is derived from "MyStruct"),
    // this returns the base type of the derivation.  For pointers, this would return the type pointed to.
    // For arrays, this would return what the array is an array of.  If the type is not such a derivative
    // type, an error is returned.
    //
    // Note that this method has nothing to do with C++ base classes.  Such are symbols which can be enumerated
    // from the derived class.
    //
    IFACEMETHOD(GetBaseType)(_Out_ ISvcSymbol **ppBaseType);

    //*************************************************
    // Internal APIs:
    //

    HRESULT RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet,
                                   _In_ ULONG64 pointerToId,
                                   _In_ SvcSymbolPointerKind pointerKind,
                                   _In_ ULONG64 reservedId = 0);

    // AppendPtrChar():
    //
    // Appends a character equivalent to the C++ (/CX) syntax for a given pointer type.
    //
    static void AppendPtrChar(_Inout_ std::wstring& str, 
                              _In_ SvcSymbolPointerKind pointerKind,
                              _In_ bool includeSpace = true)
    {
        if (includeSpace)
        {
            str += L" ";
        }

        switch(pointerKind)
        {
            case SvcSymbolPointerStandard:
                str += L"*";
                break;
            
            case SvcSymbolPointerReference:
                str += L"&";
                break;
            
            case SvcSymbolPointerRValueReference:
                str += L"&&";
                break;
            
            case SvcSymbolPointerCXHat:
                str += L"^";
                break;
        }
    }

    //*************************************************
    // Internal Accessors():
    //
    
    ULONG64 InternalGetPointerToTypeId() const { return m_pointerToId; }

private:

    ULONG64 m_pointerToId;
    SvcSymbolPointerKind m_pointerKind;

};

// ArrayTypeSymbol:
//
// A symbol which represents an array of some other type
//
class ArrayTypeSymbol :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        BaseTypeSymbol
        >
{
public:

    //*************************************************
    // ISvcSymbolType:
    //

    // GetBaseType():
    //
    // If the type is a derivation of another single type (e.g.: as "MyStruct *" is derived from "MyStruct"),
    // this returns the base type of the derivation.  For pointers, this would return the type pointed to.
    // For arrays, this would return what the array is an array of.  If the type is not such a derivative
    // type, an error is returned.
    //
    // Note that this method has nothing to do with C++ base classes.  Such are symbols which can be enumerated
    // from the derived class.
    //
    IFACEMETHOD(GetBaseType)(_Out_ ISvcSymbol **ppBaseType);

    // GetArrayDimensionality():
    //
    // Returns the dimensionality of the array.  There is no guarantee that every array type representable by
    // these interfaces is a standard zero-based one dimensional array as is standard in C.
    //
    IFACEMETHOD(GetArrayDimensionality)(_Out_ ULONG64 *pArrayDimensionality)
    {
        *pArrayDimensionality = 1;
        return S_OK;
    }

    // GetArrayDimensions():
    //
    // Fills in information about each dimension of the array including its lower bound, length, and
    // stride.
    //
    IFACEMETHOD(GetArrayDimensions)(_In_ ULONG64 dimensions,
                                    _Out_writes_(dimensions) SvcSymbolArrayDimension *pDimensions);

    // GetArrayHeaderSize():
    //
    // Gets the size of any header of the array (this is the offset of the first element of the array as
    // described by the dimensions).
    //
    // This should *ALWAYS* return 0 for a C style array.
    //
    IFACEMETHOD(GetArrayHeaderSize)(_Out_ ULONG64 *pArrayHeaderSize)
    {
        *pArrayHeaderSize = 0;
        return S_OK;
    }

    //*************************************************
    // Internal APIs:
    //

    HRESULT RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet,
                                   _In_ ULONG64 arrayOfId,
                                   _In_ ULONG64 arrayDim);

    // NotifyDependentChange():
    //
    // Called if the layout of the underlying type of the array changes, this allows us to recompute the layout
    // of the array itself.
    //
    virtual HRESULT NotifyDependentChange();

    // Delete():
    //
    // Called when this symbol is deleted.
    //
    virtual HRESULT Delete();

    //*************************************************
    // Internal Accessors():
    //
    
    ULONG64 InternalGetArrayOfTypeId() const { return m_arrayOfTypeId; }
    ULONG64 InternalGetArraySize() const { return m_arrayDim; }

private:

    ULONG64 m_arrayOfTypeId;
    ULONG64 m_arrayDim;
    ULONG64 m_baseTypeSize;

};

// TypedefTypeSymbol:
//
// A symbol which represents a typedef to some other type
//
class TypedefTypeSymbol :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        BaseTypeSymbol
        >
{
public:

    //*************************************************
    // ISvcSymbolType:
    //

    // GetBaseType():
    //
    // If the type is a derivation of another single type (e.g.: as "MyStruct *" is derived from "MyStruct"),
    // this returns the base type of the derivation.  For pointers, this would return the type pointed to.
    // For arrays, this would return what the array is an array of.  If the type is not such a derivative
    // type, an error is returned.
    //
    // Note that this method has nothing to do with C++ base classes.  Such are symbols which can be enumerated
    // from the derived class.
    //
    IFACEMETHOD(GetBaseType)(_Out_ ISvcSymbol **ppBaseType);

    //*************************************************
    // Internal APIs:
    //

    HRESULT RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet,
                                   _In_ ULONG64 typedefOfId,
                                   _In_ ULONG64 parentId,
                                   _In_ PCWSTR pwszName,
                                   _In_opt_ PCWSTR pwszQualifiedName);

    //*************************************************
    // Internal Accessors():
    //

    ULONG64 InternalGetTypedefOfTypeId() const { return m_typedefOfTypeId; }

private:

    ULONG64 m_typedefOfTypeId;

};

// EnumTypeSymbol:
//
// A symbol which represents an enum
//
class EnumTypeSymbol :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        BaseTypeSymbol
        >
{
public:

    //*************************************************
    // ISvcSymbolType:
    //

    // GetBaseType():
    //
    // If the type is a derivation of another single type (e.g.: as "MyStruct *" is derived from "MyStruct"),
    // this returns the base type of the derivation.  For pointers, this would return the type pointed to.
    // For arrays, this would return what the array is an array of.  If the type is not such a derivative
    // type, an error is returned.
    //
    // Note that this method has nothing to do with C++ base classes.  Such are symbols which can be enumerated
    // from the derived class.
    //
    IFACEMETHOD(GetBaseType)(_Out_ ISvcSymbol **ppBaseType);

    // GetIntrinsicType():
    //
    // If the type kind as reported by GetTypeKind is an intrinsic, this returns more information about
    // the particular kind of intrinsic.
    //
    IFACEMETHOD(GetIntrinsicType)(_Out_opt_ SvcSymbolIntrinsicKind *pKind,
                                  _Out_opt_ ULONG *pPackingSize)
    {
        if (pKind != nullptr)
        {
            *pKind = m_enumIntrinsicKind;
        }
        if (pPackingSize != nullptr)
        {
            *pPackingSize = static_cast<ULONG>(m_typeSize);
        }
        return S_OK;
    }

    //*************************************************
    // Internal APIs:
    //

    virtual HRESULT NotifyDependentChange()
    {
        HRESULT hr = LayoutEnum();
        if (SUCCEEDED(hr))
        {
            hr = BaseSymbol::NotifyDependentChange();
        }
        return hr;
    }

    HRESULT RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet,
                                   _In_ ULONG64 basicEnumTypeId,
                                   _In_ ULONG64 parentId,
                                   _In_ PCWSTR pwszName,
                                   _In_opt_ PCWSTR pwszQualifiedName);

    // LayoutEnum():
    //
    // Performs an enum layout.  This computes everything necessary about an enum including the values
    // of its auto-increment fields.
    //
    HRESULT LayoutEnum();

    //*************************************************
    // Internal Accessors():
    //

    ULONG64 InternalGetEnumBasicTypeId() const { return m_enumBasicTypeId; }
    VARTYPE InternalGetEnumValuePacking() const { return m_enumPacking; }
    SvcSymbolIntrinsicKind InternalGetEnumIntrinsicKind() { return m_enumIntrinsicKind; }

private:

    ULONG64 m_enumBasicTypeId;
    SvcSymbolIntrinsicKind m_enumIntrinsicKind;
    VARTYPE m_enumPacking;

};

// UdtPositionalSymbol:
//
// A symbol which is a child of a UDT that must be positioned within the UDT (e.g.: fields, base classes, etc...)
//
class UdtPositionalSymbol :
    public Microsoft::WRL::Implements<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        BaseDataSymbol
        >
{
public:

    // AutomaticAppendLayout:
    //
    // A constant used for the field offsets which indicates that this field should be automatically
    // laid out by the symbol builder.  It should just be considered appended to the end of the UDT
    // declaration with offset determined by size/alignment.
    //
    static constexpr ULONG64 AutomaticAppendLayout = static_cast<ULONG64>(-2ll);

    // AutomaticIncreaseConstantValue:
    //
    // A constant used for the field offsets which indicates that this field has a constant value and
    // no other location.  In addition, the constant value is automatically generated in the style of
    // a "C enum" where the value increases by 1 over the previous enumerant.
    //
    // This value may *ONLY* be used for an enumerant and not a general field.
    //
    static constexpr ULONG64 AutomaticIncreaseConstantValue = static_cast<ULONG64>(-3ll);

    //*************************************************
    // ISvcSymbol:
    //

    // GetOffset():
    //
    // Gets the offset of the symbol (if said symbol has such).  Note that if the symbol has multiple
    // disjoint address ranges associated with it, this method may return S_FALSE to indicate that the symbol
    // does not necessarily have a simple "base address" for an offset.
    //
    IFACEMETHOD(GetOffset)(_Out_ ULONG64 *pSymbolOffset)
    {
        //
        // Our owning type had better have done layout by now!
        //
        if (m_symOffsetActual == AutomaticAppendLayout)
        {
            return E_UNEXPECTED;
        }

        *pSymbolOffset = m_symOffsetActual;
        return S_OK;
    }

    //*************************************************
    // ISvcSymbolInfo:
    //

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
        // Our owning type had better have done layout by now!
        //
        if (m_symOffsetActual == AutomaticAppendLayout)
        {
            return E_UNEXPECTED;
        }

        pLocation->Kind = SvcSymbolLocationStructureRelative;
        pLocation->Offset = m_symOffsetActual;
        return S_OK;
    }

    //*************************************************
    // Internal APIs:
    //

    // BaseInitialize():
    //
    // Initialize the positional symbol (as an offset based symbol)
    //
    HRESULT BaseInitialize(_In_ SymbolSet *pSymbolSet,
                           _In_ SvcSymbolKind symKind,
                           _In_ ULONG64 owningTypeId,
                           _In_ ULONG64 symOffset,
                           _In_ ULONG64 symTypeId,
                           _In_opt_ PCWSTR pwszName);

    // BaseInitialize():
    //
    // Initialize the positional symbol (as a value based symbol).  Only fields can initialize in this way!
    //
    HRESULT BaseInitialize(_In_ SymbolSet *pSymbolSet,
                           _In_ ULONG64 owningTypeId,
                           _In_ VARIANT *pValue,
                           _In_ ULONG64 symTypeId,
                           _In_ PCWSTR pwszName);

    // MoveToBefore():
    //
    // Moves this positional symbol to before another one in order.  This rearranges the object's children.
    // If the object is not an automatic layout, this call doesn't do much.
    //
    HRESULT MoveToBefore(_In_ ULONG64 position);

    //*************************************************
    // Internal Accessors:
    //

    virtual bool InternalIsAutomaticLayout() const { return m_symOffset == AutomaticAppendLayout; }
    virtual bool InternalIsIncreasingConstant() const { return m_symOffset == AutomaticIncreaseConstantValue; }
    virtual bool InternalIsConstantValue() const { return m_symOffset == ConstantValue || m_symOffset == AutomaticIncreaseConstantValue; }
    virtual bool InternalIsEnumerant() const { return InternalIsConstantValue() && !InternalHasType(); }

    virtual ULONG64 InternalGetActualSymbolOffset() const { return m_symOffsetActual; }

    //*************************************************
    // Internal Computation Callbacks:
    //

    void InternalSetComputedSymbolOffset(_In_ ULONG64 offset) 
    { 
        if (m_symOffset == AutomaticAppendLayout)
        {
            m_symOffsetActual = offset;
        }
    }

    void InternalSetComputedValue(_In_ VARIANT const& value)
    {
        if (m_symOffset == AutomaticIncreaseConstantValue)
        {
            m_symValue = value;
        }
    }

protected:

    ULONG64 m_symOffsetActual;          // Either hard coded or computed from automatic layout

};
        
// FieldSymbol:
//
// A symbol which is a field of some user defined type.
//
class FieldSymbol :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        UdtPositionalSymbol
        >
{
public:

    //*************************************************
    // Internal APIs:
    //

    // RuntimeClassInitialize (normal field):
    //
    HRESULT RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet,
                                   _In_ ULONG64 owningTypeId,
                                   _In_ ULONG64 symOffset,
                                   _In_ ULONG64 symTypeId,
                                   _In_ PCWSTR pwszName)
    {
        return UdtPositionalSymbol::BaseInitialize(pSymbolSet,
                                                   SvcSymbolField,
                                                   owningTypeId,
                                                   symOffset,
                                                   symTypeId,
                                                   pwszName);
    }

    // RuntimeClassInitialize (constant valued field):
    //
    // NOTE: An enumerant may legally pass "0" as symTypeId.  It inherits this from the enum
    //       itself and each enumerant does *NOT* need a separate type.
    //
    HRESULT RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet,
                                   _In_ ULONG64 owningTypeId,
                                   _In_ ULONG64 symTypeId,
                                   _In_ VARIANT *pValue,
                                   _In_ PCWSTR pwszName)
    {
        return UdtPositionalSymbol::BaseInitialize(pSymbolSet,
                                                   owningTypeId,
                                                   pValue,
                                                   symTypeId,
                                                   pwszName);
    }
};

// BaseClassSymbol:
//
// A symbol which is a base class of some user defined type.
//
class BaseClassSymbol :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        UdtPositionalSymbol
        >
{
public:

    //*************************************************
    // Internal APIs:
    //

    HRESULT RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet,
                                   _In_ ULONG64 owningTypeId,
                                   _In_ ULONG64 symOffset,
                                   _In_ ULONG64 symTypeId)
    {
        return UdtPositionalSymbol::BaseInitialize(pSymbolSet,
                                                   SvcSymbolBaseClass,
                                                   owningTypeId,
                                                   symOffset,
                                                   symTypeId,
                                                   nullptr);
    }
};

// FunctionTypeSymbol:
//
// A symbol representing a function type.
//
class FunctionTypeSymbol :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        BaseTypeSymbol
        >
{
public:

    //*************************************************
    // ISvcSymbolType:
    //

    // GetTypeKind():
    //
    // Gets the kind of type symbol that this is (e.g.: base type, struct, array, etc...)
    //
    IFACEMETHOD(GetTypeKind)(_Out_ SvcSymbolTypeKind *pTypeKind)
    {
        *pTypeKind = SvcSymbolTypeFunction;
        return S_OK;
    }

    // GetFunctionReturnType():
    //
    // Returns the return type of a function.  Even non-value returning functions (e.g.: void) should return
    // a type representing this.
    //
    IFACEMETHOD(GetFunctionReturnType)(_COM_Outptr_ ISvcSymbol **ppReturnType);

    // GetFunctionParameterTypeCount():
    //
    // Returns the number of parameters that the function takes.
    //
    IFACEMETHOD(GetFunctionParameterTypeCount)(_Out_ ULONG64 *pCount)
    {
        *pCount = m_paramTypes.size();
        return S_OK;
    }

    // GetFunctionParameterTypeAt():
    //
    // Returns the type of the "i"-th argument to the function as a new ISvcSymbol
    //
    IFACEMETHOD(GetFunctionParameterTypeAt)(_In_ ULONG64 i,
                                            _COM_Outptr_ ISvcSymbol **ppParameterType);


    //*************************************************
    // Internal APIs:
    //

    // RuntimeClassInitialize():
    //
    // Initializes a fully formed function type symbol (with the return types and paramter types known
    // upfront).
    //
    HRESULT RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet,
                                   _In_ ULONG64 returnTypeId,
                                   _In_ ULONG64 paramCount,
                                   _In_reads_(paramCount) ULONG64 *pParamTypes);

    // RuntimeClassInitialize():
    //
    // Initializes a shell function type symbol (where return types and parameter types will be filled
    // in during an import or other similar operation).
    //
    HRESULT RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet);

    // InternalSetReturnType():
    //
    // Sets the return type of the function type symbol once it has been imported.
    //
    void InternalSetReturnType(_In_ ULONG64 returnType)
    {
        m_returnType = returnType;
    }

    // InternalSetParameterTypes():
    //
    // Sets the parameter types of the function type symbol once they have been imported.
    //
    HRESULT InternalSetParameterTypes(_In_ ULONG64 paramCount,
                                      _In_reads_(paramCount) ULONG64 *pParamTypes);

private:

    ULONG64 m_returnType;
    std::vector<ULONG64> m_paramTypes;
};

} // SymbolBuilder
} // Services
} // TargetComposition
} // Debugger

#endif // __SYMBOLTYPES_H__
