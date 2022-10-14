//**************************************************************************
//
// ApiProvider.h
//
// Header for a provider for the data model which extends Debugger.Utility.*
// to create new APIs to access our underlying functionality.
//
// The provider here is implemented with the C++17 data model client library.
// Effectively, there are a series of classes whose constructors bind certain C++ methods
// to the data model as property getters and setters or method callbacks.
//
// The global ApiProvider object houses all of these classes.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#ifndef __APIPROVIDER_H__
#define __APIPROVIDER_H__

namespace Debugger
{
namespace DataModel
{
namespace Libraries
{
namespace SymbolBuilder
{

using namespace Debugger::DataModel::ClientEx;
using namespace Debugger::DataModel::ProviderEx;
using namespace Debugger::TargetComposition::Services::SymbolBuilder;
using namespace Microsoft::WRL;

//*************************************************
// Helpers:
//

// SymbolObjectHelpers:
//
// Base class providing some general helpers for various boxed symbols.
//
class SymbolObjectHelpers
{
public:

    // UnboxType():
    //
    // Takes an object which may be a type name or one of the various type objects we created and unboxes it
    // to a BaseTypeSymbol.  Note that no reference count is returned on the resulting pointer.  It is valid
    // solely by the fact that it is in the global symbol table of the symbol set.
    //
    BaseTypeSymbol *UnboxType(_In_ SymbolSet *pSymbolSet, _In_ Object typeObject, _In_ bool allowAutoCreations = true);

    // BoxRelatedType():
    //
    // Takes one of our types as identified by a type id (related to an existing symbol; for example, the
    // base type of a pointer) and boxes it into the data model by finding and calling the appropriate factory object.
    //
    Object BoxRelatedType(_In_ BaseSymbol *pSymbol, _In_ ULONG64 typeId);

    // BoxType():
    //
    // Takes one of our types and boxes it into the data model by finding and calling the appropriate factory
    // object.
    //
    Object BoxType(_In_ BaseTypeSymbol *pTypeSymbol);

    // BoxSymbol():
    //
    // Takes one of our symbols and boxes it into the data model by finding and calling the appropriate factory
    // object.
    //
    Object BoxSymbol(_In_ BaseSymbol *pSymbol);
};

// SymbolSetObject:
//
// Represents one of our symbol set objects boxed into the data model.
//
class SymbolSetObject : public TypedInstanceModel<ComPtr<SymbolSet>>
{
public:

    SymbolSetObject();

private:

    // GetTypes():
    //
    // Property accessor which gets the types on this symbol set.
    //
    Object GetTypes(_In_ const Object& /*symbolSetObject*/, _In_ ComPtr<SymbolSet>& spSymbolSet);

    // GetData():
    //
    // Property accessor which gets the data on this symbol set.
    //
    Object GetData(_In_ const Object& /*symbolSetObject*/, _In_ ComPtr<SymbolSet>& spSymbolSet);

    // GetFunctions():
    //
    // Property accessor which gets the functions on this symbol set.
    //
    Object GetFunctions(_In_ const Object& /*symbolSetObject*/, _In_ ComPtr<SymbolSet>& spSymbolSet);

    // GetPublics():
    //
    // Property accessor which gets the public symbols on this symbol set.
    //
    Object GetPublics(_In_ const Object& /*symbolSetObject*/, _In_ ComPtr<SymbolSet>& spSymbolSet);

};

//*************************************************
// Base Symbols:
//

// BaseSymbolObject:
//
// Base class for our symbol objects which provides helpers and some basic bindings.
//
template<typename TSym>
class BaseSymbolObject : public TypedInstanceModel<ComPtr<TSym>>,
                         public SymbolObjectHelpers
{
public:

    BaseSymbolObject();

protected:

    // GetName():
    //
    // Bound property accessor to get the name of the symbol.
    //
    Object GetName(_In_ const Object& symbolObject, _In_ ComPtr<TSym>& spSymbol);

    // GetQualifiedName():
    //
    // Bound property accessor to get the qualified name of the symbol.
    //
    Object GetQualifiedName(_In_ const Object& symbolObject, _In_ ComPtr<TSym>& spSymbol);

    // GetParent():
    //
    // Bound property accessor to get the parent of the symbol.
    //
    Object GetParent(_In_ const Object& symbolObject, _In_ ComPtr<TSym>& spSymbol);

private:
};

//*************************************************
// Types:
//

// BaseTypeObject:
//
// Base class for various types boxed into the data model.
//
template<typename TType>
class BaseTypeObject : public BaseSymbolObject<TType>
{
public:

    BaseTypeObject(_In_ PCWSTR pwszConvTag);

protected:

    // GetSize():
    //
    // Bound property accessor which gets the size of the type.
    //
    ULONG64 GetSize(_In_ const Object& typeObject, _In_ ComPtr<TType>& spTypeSymbol);

    // GetAlignment():
    //
    // Bound property accessor which gets the alignment of the type.
    //
    ULONG64 GetAlignment(_In_ const Object& typeObject, _In_ ComPtr<TType>& spTypeSymbol);

    // ToString():
    //
    // Bound function that is the string conversion for a type.
    //
    std::wstring ToString(_In_ const Object& typeObject,
                          _In_ ComPtr<TType>& spTypeSymbol,
                          _In_ const Metadata& metadata);

    // Delete():
    //
    // Bound method which will delete a type.
    //
    void Delete(_In_ const Object& typeObject, _In_ ComPtr<TType>& spTypeSymbol);

private:

    // String conversion tag.
    PCWSTR m_pwszConvTag;

};

// TypesObject:
//
// Represents the list of types (and type APIs) available on a symbol set.
//
class TypesObject : public TypedInstanceModel<ComPtr<SymbolSet>>,
                    public SymbolObjectHelpers
{
public:

    TypesObject();

private:

    // AddBasicCTypes():
    //
    // Bound API which will add basic C types to the symbol set with standard Windows definitions.
    //
    void AddBasicCTypes(_In_ const Object& typesObject, _In_ ComPtr<SymbolSet>& spSymbolSet);

    // Create():
    //
    // Bound API which will create a new UDT and return an object representing it.
    //
    Object Create(_In_ const Object& typesObject, 
                  _In_ ComPtr<SymbolSet>& spSymbolSet,
                  _In_ std::wstring typeName,
                  _In_ std::optional<std::wstring> qualifiedTypeName);

    // CreatePointer():
    //
    // Bound API which will create a new pointer to something and return an object representing it.
    //
    Object CreatePointer(_In_ const Object& typesObject,
                         _In_ ComPtr<SymbolSet>& spSymbolSet,
                         _In_ Object pointedToType);

    // CreateArray():
    //
    // Bound API which will create a new array of something and return an object representing it.
    //
    Object CreateArray(_In_ const Object& typesObject,
                       _In_ ComPtr<SymbolSet>& spSymbolSet,
                       _In_ Object arrayOfType,
                       _In_ ULONG64 arrayDim);

    // CreateTypedef():
    //
    // Bound API which will create a new typedef of something and return an object representing it.
    //
    Object CreateTypedef(_In_ const Object& typesObject,
                         _In_ ComPtr<SymbolSet>& spSymbolSet,
                         _In_ std::wstring typeName,
                         _In_ Object typedefOfType,
                         _In_ std::optional<std::wstring> qualifiedTypeName);

    // CreateEnum():
    //
    // Bound API which will create a new enum of something and return an object representing it.
    //
    Object CreateEnum(_In_ const Object& typesObject,
                      _In_ ComPtr<SymbolSet>& spSymbolSet,
                      _In_ std::wstring typeName,
                      _In_ std::optional<Object> enumBasicType,
                      _In_ std::optional<std::wstring> qualifiedTypeName);

    // GetIterator():
    //
    // Bound generator for iterating over types within a symbol set.
    //
    std::experimental::generator<Object> GetIterator(_In_ const Object& typesObject,
                                                     _In_ ComPtr<SymbolSet>& spSymbolSet);

};


// UdtTypeObject:
//
// Represents one of our UDT symbol objects boxed into the data model.
//
class UdtTypeObject : public BaseTypeObject<UdtTypeSymbol>
{
public:

    UdtTypeObject();

private:

    // GetBaseClasses()
    //
    // Property accessor which gets the base classes on this UDT.
    //
    Object GetBaseClasses(_In_ const Object& /*typeObject*/, _In_ ComPtr<UdtTypeSymbol>& spUdtTypeSymbol);

    // GetFields():
    //
    // Property accessor which gets the fields on this UDT.
    //
    Object GetFields(_In_ const Object& /*typeObject*/, _In_ ComPtr<UdtTypeSymbol>& spUdtTypeSymbol);

};

// BasicTypeObject:
//
// Represents one of our basic (intrinsic type) symbol objects boxed into the data model.
//
class BasicTypeObject : public BaseTypeObject<BasicTypeSymbol>
{
public:

    BasicTypeObject();

};

// PointerTypeObject:
//
// Represents a pointer type symbol boxed into the data model.
//
class PointerTypeObject : public BaseTypeObject<PointerTypeSymbol>
{
public:

    PointerTypeObject();

private:

    // GetBaseType():
    //
    // Property accessor which gets the base type of this pointer (what it points to)
    //
    Object GetBaseType(_In_ const Object& /*typeObject*/, _In_ ComPtr<PointerTypeSymbol>& spPointerTypeSymbol);
};

// ArrayTypeObject:
//
// Represents an array type symbol boxed into the data model.
//
class ArrayTypeObject : public BaseTypeObject<ArrayTypeSymbol>
{
public:

    ArrayTypeObject();

private:

    // GetBaseType():
    //
    // Property accessor which gets the base type of this array (what it is an array of)
    //
    Object GetBaseType(_In_ const Object& /*arrayObject*/, _In_ ComPtr<ArrayTypeSymbol>& spArrayTypeSymbol);

    // GetArraySize():
    //
    // Bound property accessor which returns the size of the array (number of elements).
    //
    ULONG64 GetArraySize(_In_ const Object& arrayObject, _In_ ComPtr<ArrayTypeSymbol>& spArraySymbol);
};

// TypedefTypeObject:
//
// Represenhts a typedef type symbol boxed into the data model.
//
class TypedefTypeObject : public BaseTypeObject<TypedefTypeSymbol>
{
public:

    TypedefTypeObject();

private:

    // GetBaseType():
    //
    // Property accessor which gets the base type of this typedef (what it is a typedef of)
    //
    Object GetBaseType(_In_ const Object& /*typedefObject*/, _In_ ComPtr<TypedefTypeSymbol>& spTypedefTypeSymbol);
};

// EnumTypeObject:
//
// Represenhts an enum type symbol boxed into the data model.
//
class EnumTypeObject : public BaseTypeObject<EnumTypeSymbol>
{
public:

    EnumTypeObject();

private:

    // GetBaseType():
    //
    // Property accessor which gets the base type of this enum (the basic type of the enum)
    //
    Object GetBaseType(_In_ const Object& /*enumObject*/, _In_ ComPtr<EnumTypeSymbol>& spEnumTypeSymbol);

    // GetEnumerants():
    //
    // Property accessor which gets the list of enumerants within this enum (the values)
    //
    Object GetEnumerants(_In_ const Object& /*enumObject*/, _In_ ComPtr<EnumTypeSymbol>& spEnumTypeSymbol);
};

// FieldsObject:
//
// Represents the list of fields (and APIs) available on a UDT type.
//
class FieldsObject : public TypedInstanceModel<ComPtr<UdtTypeSymbol>>,
                     public SymbolObjectHelpers
{
public:

    FieldsObject();

private:

    // Add():
    //
    // Adds a new field to the UDT.  The 'fieldType' may either be a fully qualified type name
    // or may be a type object returned from something like SymbolSet.CreateType().  If the 'fieldOffset'
    // is supplied, the field will be placed specifically at that offset.  If it is not, the field will be
    // appended to the end of the UDT (+ requisite alignment padding) and will alter the UDTs size and layout.
    // 
    Object Add(_In_ const Object& typeObject,
               _In_ ComPtr<UdtTypeSymbol>& spUdtTypeSymbol,
               _In_ std::wstring fieldName,
               _In_ Object fieldType,
               _In_ std::optional<ULONG64> fieldOffset);

    // GetIterator():
    //
    // Bound generator for iterating over fields within a type.
    //
    std::experimental::generator<Object> GetIterator(_In_ const Object& fieldsObject,
                                                     _In_ ComPtr<UdtTypeSymbol>& spUdtTypeSymbol);

};

// EnumerantsObject:
//
// Represents the list of enumerants (and APIs) available on an enum type.  This is slightly different than
// the more general "fields" concept because for enums, everything must be a constant valued "enumerant" (really
// a field) that matches the underlying enum type.
//
class EnumerantsObject : public TypedInstanceModel<ComPtr<EnumTypeSymbol>>,
                         public SymbolObjectHelpers
{
public:

    EnumerantsObject();

private:

    // Add():
    //
    // Adds a new enumerant to the enum.  If the value is unspecified, it acts as a "C enum" where it will
    // auto-increment from the previous enumerant value.
    //
    Object Add(_In_ const Object& enumObject,
               _In_ ComPtr<EnumTypeSymbol>& spEnumTypeSymbol,
               _In_ std::wstring enumerantName,
               _In_ std::optional<Object> enumerantValue);

    // GetIterator():
    //
    // Bound generator for iterating over enumerants within an enum.
    //
    std::experimental::generator<Object> GetIterator(_In_ const Object& enumObject,
                                                     _In_ ComPtr<EnumTypeSymbol>& spEnumTypeSymbol);


};

// FieldObject:
//
// Represents one of our field objects boxed into the data model.
//
class FieldObject : public BaseSymbolObject<FieldSymbol>
{
public:

    FieldObject();

private:

    // [Get/Set]IsAutomaticLayout():
    //
    // Bound property accessor which determines whether the field is automatic layout or not.
    //
    std::optional<bool> GetIsAutomaticLayout(_In_ const Object& fieldObject, _In_ ComPtr<FieldSymbol>& spFieldSymbol);
    void SetIsAutomaticLayout(_In_ const Object& fieldObject,
                              _In_ ComPtr<FieldSymbol>& spFieldSymbol,
                              _In_ bool isAutomaticLayout);

    // [Get/Set]Type():
    //
    // Bound property accessor which returns the type of the field.
    //
    Object GetType(_In_ const Object& fieldObject, _In_ ComPtr<FieldSymbol>& spFieldSymbol);
    void SetType(_In_ const Object& fieldObject, _In_ ComPtr<FieldSymbol>& spFieldSymbol,
                 _In_ Object fieldType);

    // [Get/Set]Offset():
    //
    // Bound property accessor which returns the offset of the field.
    //
    std::optional<ULONG64> GetOffset(_In_ const Object& fieldObject, _In_ ComPtr<FieldSymbol>& spFieldSymbol);
    void SetOffset(_In_ const Object& fieldObject, _In_ ComPtr<FieldSymbol>& spFieldSymbol,
                   _In_ ULONG64 fieldOffset);

    // Delete():
    //
    // Bound method which will delete a field of a type.
    //
    void Delete(_In_ const Object& fieldObject, _In_ ComPtr<FieldSymbol>& spFieldSymbol);

    // MoveBefore():
    //
    // Bound method which moves the field to before another field.  The 'beforeObj' may either be another
    // field or it may be the numeric index of a field.
    //
    void MoveBefore(_In_ const Object& fieldObject, _In_ ComPtr<FieldSymbol>& spFieldSymbol,
                    _In_ Object beforeObj);

    // ToString():
    //
    // Bound function that is the string conversion for a UDT field.
    //
    std::wstring ToString(_In_ const Object& fieldObject,
                          _In_ ComPtr<FieldSymbol>& spFieldSymbol,
                          _In_ const Metadata& metadata);

};

// BaseClassesObject:
//
// Represents the list of base classes (and APIs) available on a UDT type.
//
class BaseClassesObject : public TypedInstanceModel<ComPtr<UdtTypeSymbol>>
{
public:

    BaseClassesObject();

private:

    // Add():
    //
    // Adds a new base class to the UDT.  The 'fieldType' may either be a fully qualified type name
    // or may be a type object returned from something like SymbolSet.CreateType().  If the 'baseClassOffset'
    // is supplied, the field will be placed specifically at that offset.  If it is not, the field will be
    // appended to the end of the base class list (+ requisite alignment padding) and will alter the UDTs size 
    // and layout.
    // 
    Object Add(_In_ const Object& typeObject,
               _In_ ComPtr<UdtTypeSymbol>& spUdtTypeSymbol,
               _In_ Object baseClassType,
               _In_ std::optional<ULONG64> baseClassOffset);

    // GetIterator():
    //
    // Bound generator for iterating over fields within a type.
    //
    std::experimental::generator<Object> GetIterator(_In_ const Object& baseClassesObject,
                                                     _In_ ComPtr<UdtTypeSymbol>& spUdtTypeSymbol);

};

// BaseClassObject:
//
// Represents one of our base class objects boxed into the data model.
//
class BaseClassObject : public BaseSymbolObject<BaseClassSymbol>
{
public:

    BaseClassObject();

private:

    // [Get/Set]IsAutomaticLayout():
    //
    // Bound property accessor which determines whether the base class is automatic layout or not.
    //
    bool GetIsAutomaticLayout(_In_ const Object& baseClassObject, _In_ ComPtr<BaseClassSymbol>& spBaseClassSymbol);
    void SetIsAutomaticLayout(_In_ const Object& baseClassObject,
                              _In_ ComPtr<BaseClassSymbol>& spBaseClassSymbol,
                              _In_ bool isAutomaticLayout);

    // [Get/Set]Type():
    //
    // Bound property accessor which returns the type of the base class.
    //
    Object GetType(_In_ const Object& baseClassObject, _In_ ComPtr<BaseClassSymbol>& spBaseClassSymbol);
    void SetType(_In_ const Object& baseClassObject, _In_ ComPtr<BaseClassSymbol>& spBaseClassSymbol,
                 _In_ Object baseClassType);

    // [Get/Set]Offset():
    //
    // Bound property accessor which returns the offset of the base class.
    //
    ULONG64 GetOffset(_In_ const Object& baseClassObject, _In_ ComPtr<BaseClassSymbol>& spBaseClassSymbol);
    void SetOffset(_In_ const Object& baseClassObject, _In_ ComPtr<BaseClassSymbol>& spBaseClassSymbol,
                   _In_ ULONG64 baseClassOffset);

    // Delete():
    //
    // Bound method which will delete a base class of a type.
    //
    void Delete(_In_ const Object& baseClassObject, _In_ ComPtr<BaseClassSymbol>& spBaseClassSymbol);

    // MoveBefore():
    //
    // Bound method which moves the base class to before another base class.  The 'beforeObj' may either be another
    // base class or it may be the numeric index of a base class.
    //
    void MoveBefore(_In_ const Object& baseClassObject, _In_ ComPtr<BaseClassSymbol>& spBaseClassSymbol,
                    _In_ Object beforeObj);

    // ToString():
    //
    // Bound function that is the string conversion for a UDT field.
    //
    std::wstring ToString(_In_ const Object& baseClassObject,
                          _In_ ComPtr<BaseClassSymbol>& spBaseClassSymbol,
                          _In_ const Metadata& metadata);

};

//*************************************************
// Data:
//

// DataObject:
//
// Represents the list of data (and data APIs) available on a symbol set.
//
class DataObject : public TypedInstanceModel<ComPtr<SymbolSet>>,
                   public SymbolObjectHelpers
{
public:

    DataObject();

private:

    // CreateGlobal():
    //
    // Bound API which will create a new piece of global data and return an object representing it.
    //
    Object CreateGlobal(_In_ const Object& typesObject, 
                        _In_ ComPtr<SymbolSet>& spSymbolSet,
                        _In_ std::wstring dataName,
                        _In_ Object dataType,
                        _In_ ULONG64 dataOffset,
                        _In_ std::optional<std::wstring> qualifiedDataName);

    // GetIterator():
    //
    // Bound generator for iterating over data within a symbol set.
    //
    std::experimental::generator<Object> GetIterator(_In_ const Object& dataObject,
                                                     _In_ ComPtr<SymbolSet>& spSymbolSet);

};

// GlobalDataObject:
//
// Represents a global data objects boxed into the data model.
//
class GlobalDataObject : public BaseSymbolObject<GlobalDataSymbol>
{
public:

    GlobalDataObject();

private:

    // [Get/Set]Type():
    //
    // Bound property accessor which returns the type of the global data.
    //
    Object GetType(_In_ const Object& globalDataObject, _In_ ComPtr<GlobalDataSymbol>& spGlobalDataSymbol);
    void SetType(_In_ const Object& globalDataObject, _In_ ComPtr<GlobalDataSymbol>& spGlobalDataSymbol,
                 _In_ Object globalDataType);

    // [Get/Set]Offset():
    //
    // Bound property accessor which returns the offset of the global data.
    //
    std::optional<ULONG64> GetOffset(_In_ const Object& globalDataObject, 
                                     _In_ ComPtr<GlobalDataSymbol>& spGlobalDataSymbol);
    void SetOffset(_In_ const Object& globalDataObject, _In_ ComPtr<GlobalDataSymbol>& spGlobalDataSymbol,
                   _In_ ULONG64 globalDataOffset);

    // ToString():
    //
    // Bound function that is the string conversion for global data.
    //
    std::wstring ToString(_In_ const Object& globalDataObject,
                          _In_ ComPtr<GlobalDataSymbol>& spGlobalDataSymbol,
                          _In_ const Metadata& metadata);

    // Delete():
    //
    // Bound method which will delete global data.
    //
    void Delete(_In_ const Object& globalDataObject, _In_ ComPtr<GlobalDataSymbol>& spGlobalDataSymbol);
};

//*************************************************
// Functions:
//

// FunctionsObject:
//
// Represents the list of functions (and function APIs) available on a symbol set.
//
class FunctionsObject : public TypedInstanceModel<ComPtr<SymbolSet>>,
                        public SymbolObjectHelpers
{
public:

    FunctionsObject();

private:

    // Create():
    //
    // Bound API which will create a new function and return an object representing it.
    //
    Object Create(_In_ const Object& typesObject, 
                  _In_ ComPtr<SymbolSet>& spSymbolSet,
                  _In_ std::wstring functionName,
                  _In_ Object returnType,
                  _In_ ULONG64 codeOffset,
                  _In_ ULONG64 codeSize,
                  _In_ size_t argCount,                 // [qualifiedName], [parameter]...
                  _In_reads_(argCount) Object *pArgs);

    // GetIterator():
    //
    // Bound generator for iterating over functions within a symbol set.
    //
    std::experimental::generator<Object> GetIterator(_In_ const Object& functionsObject,
                                                     _In_ ComPtr<SymbolSet>& spSymbolSet);

};

// FunctionObject:
//
// Represents a function boxed into the data model.
//
class FunctionObject : public BaseSymbolObject<FunctionSymbol>
{
public:

    FunctionObject();
	
    // ToString():
    //
    // Bound function that is the string conversion for functions.
    //
    std::wstring ToString(_In_ const Object& functionObject,
                          _In_ ComPtr<FunctionSymbol>& spFunctionSymbol,
                          _In_ const Metadata& metadata);

    // [Get/Set]ReturnType():
    //
    // Bound property accessor which returns the return type of the function.
    //
    Object GetReturnType(_In_ const Object& functionObject, 
                         _In_ ComPtr<FunctionSymbol>& spFunctionSymbol);
    void SetReturnType(_In_ const Object& functionObject, 
                       _In_ ComPtr<FunctionSymbol>& spFunctionSymbol,
                       _In_ Object returnType);

    // GetLocalVariables():
    //
    // Property accessor which gets the local variables of this function.
    //
    Object GetLocalVariables(_In_ const Object& /*functionObject*/, _In_ ComPtr<FunctionSymbol>& spFunctionSymbol);

    // GetParameters():
    //
    // Property accessor which gets the parameters of this function.
    //
    Object GetParameters(_In_ const Object& /*functionObject*/, _In_ ComPtr<FunctionSymbol>& spFunctionSymbol);

};

// ParametersObject:
//
// Represents the list of parameters (and APIs) available on a function.
//
class ParametersObject : public TypedInstanceModel<ComPtr<FunctionSymbol>>,
                         public SymbolObjectHelpers
{
public:

    ParametersObject();

private:

    // Add():
    //
    // Adds a new parameter to the function.  The 'parameterType' may either be a fully qualified 
    // type name or may be a type object returned from something like SymbolSet.CreateType().  
    // The newly created parameter will be added at the end of the current parameter list.
    // 
    Object Add(_In_ const Object& parametersObject,
               _In_ ComPtr<FunctionSymbol>& spFunctionSymbol,
               _In_ std::wstring parameterName,
               _In_ Object parameterType);

    // GetIterator():
    //
    // Bound generator for iterating over parameters within a function.
    //
    std::experimental::generator<Object> GetIterator(_In_ const Object& parametersObject,
                                                     _In_ ComPtr<FunctionSymbol>& spFunctionSymbol);

    // ToString():
    //
    // Bound function that is the string conversion for parameters
    //
    std::wstring ToString(_In_ const Object& parametersObject,
                          _In_ ComPtr<FunctionSymbol>& spFunctionSymbol,
                          _In_ const Metadata& metadata);

};

// LocalVariablesObject:
//
// Represents the list of local variables (and APIs) available on a function.
//
class LocalVariablesObject : public TypedInstanceModel<ComPtr<FunctionSymbol>>,
                             public SymbolObjectHelpers
{
public:

    LocalVariablesObject();

private:

    // Add():
    //
    // Adds a new local variable to the function.  The 'localVariableType' may either be a fully 
    // qualified  type name or may be a type object returned from something like 
    // SymbolSet.CreateType().  
    // 
    Object Add(_In_ const Object& localVariablesObject,
               _In_ ComPtr<FunctionSymbol>& spFunctionSymbol,
               _In_ std::wstring localVariableName,
               _In_ Object localVariableType);

    // GetIterator():
    //
    // Bound generator for iterating over local variables within a function.
    //
    std::experimental::generator<Object> GetIterator(_In_ const Object& localVariablesObject,
                                                     _In_ ComPtr<FunctionSymbol>& spFunctionSymbol);

};

// BaseVariableObject:
//
// A base class for variable objects boxed into the data model.  This may represent a parameter
// or a local variable.
//
class BaseVariableObject : public BaseSymbolObject<VariableSymbol>
{
public:

    BaseVariableObject();

protected:

    // [Get/Set]Name():
    //
    // Bound property accessor which returns the name of the variable.
    //
    Object GetName(_In_ const Object& variableObject, _In_ ComPtr<VariableSymbol>& spVariableSymbol);
    void SetName(_In_ const Object& variableObject, _In_ ComPtr<VariableSymbol>& spVariableSymbol,
                 _In_ std::wstring variableName);

    // [Get/Set]Type():
    //
    // Bound property accessor which returns the type of the variable.
    //
    Object GetType(_In_ const Object& variableObject, _In_ ComPtr<VariableSymbol>& spVariableSymbol);
    void SetType(_In_ const Object& variableObject, _In_ ComPtr<VariableSymbol>& spVariableSymbol,
                 _In_ Object variableType);

    // GetLiveRanges():
    //
    // Bound property accessor which returns the list of live ranges for this variable.
    //
    Object GetLiveRanges(_In_ const Object& variableObject, _In_ ComPtr<VariableSymbol>& spVariableSymbol);

    // Delete():
    //
    // Bound method which will delete a function variable
    //
    void Delete(_In_ const Object& variableObject, _In_ ComPtr<VariableSymbol>& spVariableSymbol);

    // ToString():
    //
    // Bound function that is the string conversion for a function variable.
    //
    std::wstring ToString(_In_ const Object& variableObject,
                          _In_ ComPtr<VariableSymbol>& spVariableSymbol,
                          _In_ const Metadata& metadata);
};

// ParameterObject:
//
// Represents a function parameter boxed into the data model.
//
class ParameterObject : public BaseVariableObject
{
public:

    ParameterObject();

private:


    // MoveBefore():
    //
    // Bound method which moves the parameter to before another parameter.  The 'beforeObj' 
    // may either be another parameter or it may be the numeric index of a parameter.
    //
    void MoveBefore(_In_ const Object& parameterObject, _In_ ComPtr<VariableSymbol>& spParameterSymbol,
                    _In_ Object beforeObj);


};

// LocalVariableObject:
//
// Represents a local variable boxed into the data model.
//
class LocalVariableObject : public BaseVariableObject
{
public:

    LocalVariableObject();
};

// LiveRangesObject:
//
// Represents the list of live ranges for a given parameter or local variable of a function.
//
class LiveRangesObject : public TypedInstanceModel<ComPtr<VariableSymbol>>,
                         public SymbolObjectHelpers
{
public:

    LiveRangesObject();

private:

    // Add():
    //
    // Adds a new live range to the parameter or local variable. 
    // 
    Object Add(_In_ const Object& liveRangesObject,
               _In_ ComPtr<VariableSymbol>& spVariableSymbol,
               _In_ ULONG64 rangeOffset,
               _In_ ULONG64 rangeSize,
               _In_ std::wstring locDesc);

    // GetIterator():
    //
    // Bound generator for iterating over parameters within a function.
    //
    std::experimental::generator<Object> GetIterator(_In_ const Object& liveRangesObject,
                                                     _In_ ComPtr<VariableSymbol>& spVariableSymbol);
};

struct LiveRangeInformation
{
    ComPtr<VariableSymbol> Variable;
    ULONG64 LiveRangeIdentity;
};

// LiveRangeObject:
//
// Represents a particular live range for a variable within a function.
//
class LiveRangeObject : public TypedInstanceModel<LiveRangeInformation>
{
public:

    LiveRangeObject();

private:

    // [Get/Set]Offset
    //
    // Gets/sets the offset of the live range into the function.
    //
    ULONG64 GetOffset(_In_ const Object& liveRangeObject, 
                      _In_ LiveRangeInformation const& liveRangeInfo);
    void SetOffset(_In_ const Object& liveRangeObject, 
                   _In_ LiveRangeInformation const& liveRangeInfo,
                   _In_ ULONG64 offset);

    // [Get/Set]Size
    //
    // Gets/sets the size of the live range
    //
    ULONG64 GetSize(_In_ const Object& liveRangeObject,
                    _In_ LiveRangeInformation const& liveRangeInfo);
    void SetSize(_In_ const Object& liveRangeObject,
                 _In_ LiveRangeInformation const& liveRangeInfo,
                 _In_ ULONG64 size);

    // [Get/Set]Location
    //
    // Gets/sets the location of the live range as a string description.
    //
    std::wstring GetLocation(_In_ const Object& liveRangeObject,
                             _In_ LiveRangeInformation const& liveRangeInfo);
    void SetLocation(_In_ const Object& liveRangeObject,
                     _In_ LiveRangeInformation const& liveRangeInfo,
                     _In_ std::wstring location);

    // Delete():
    //
    // Bound method which will delete a live range.
    //
    void Delete(_In_ const Object& liveRangeObject, _In_ LiveRangeInformation const& liveRangeInfo);

    // ToString():
    //
    // Bound function that is the string conversion for a live range.
    //
    std::wstring ToString(_In_ const Object& liveRangeObject,
                          _In_ LiveRangeInformation const& liveRangeInfo,
                          _In_ const Metadata& metadata);

};

//*************************************************
// Publics:
//

// PublicsObject:
//
// Represents the list of public symbols (and public symbol APIs) available on a symbol set.
//
class PublicsObject : public TypedInstanceModel<ComPtr<SymbolSet>>,
                      public SymbolObjectHelpers
{
public:

    PublicsObject();

private:

    // Create():
    //
    // Bound API which will create a new public symbol and return an object representing it.
    //
    Object Create(_In_ const Object& typesObject, 
                  _In_ ComPtr<SymbolSet>& spSymbolSet,
                  _In_ std::wstring publicName,
                  _In_ ULONG64 publicOffset);

    // GetIterator():
    //
    // Bound generator for iterating over public symbols within a symbol set.
    //
    std::experimental::generator<Object> GetIterator(_In_ const Object& publicsObject,
                                                     _In_ ComPtr<SymbolSet>& spSymbolSet);

};

// PublicObject:
//
// Represents a public symbol boxed into the data model.
//
class PublicObject : public BaseSymbolObject<PublicSymbol>
{
public:

    PublicObject();
	
    // ToString():
    //
    // Bound function that is the string conversion for public symbols.
    //
    std::wstring ToString(_In_ const Object& publicbject,
                          _In_ ComPtr<PublicSymbol>& spPublicSymbol,
                          _In_ const Metadata& metadata);

    // GetOffset():
    //
    // Bound property accessor which returns the offset of the public symbol.
    // 
    ULONG64 GetOffset(_In_ const Object& publicObject, _In_ ComPtr<PublicSymbol>& spPublicSymbol);

};

//*************************************************
// Extension Points:
//

// ModuleExtension:
//
// An extension on the debugger's notion of a module which presents any symbol builder symbols associated
// with the given module.
//
class ModuleExtension : public ExtensionModel
{
public:
    
    ModuleExtension();

private:

    // GetSymbolBuilderSymbols():
    //
    // Bound property accessor which retrieves the symbol builder symbols associated with the given module.
    //
    Object GetSymbolBuilderSymbols(_In_ const Object& moduleObject);

};

// SymbolBuilderNamespace:
//
// This is what provides *OUR* library's additions to <API>.SymbolBuilder
//
class SymbolBuilderNamespace : public ExtensionModel
{
public:

    // SymbolBuilderNamespace()
    //
    // Creates the symbol builder namespace
    //
    SymbolBuilderNamespace();

private:

    // CreateSymbols():
    //
    // Creates a new symbol builder set for a given module.  We support several forms of CreateSymbols():
    //
    //     CreateSymbols(moduleObject, [options])     // moduleObject is @$curprocess.Modules[N]
    //     CreateSymbols(moduleBase, [options])       // moduleBase is the base address of a module in the current process context
    //     CreateSymbols(moduleName, [options])       // moduleName is the name of a module in the current process context
    //
    Object CreateSymbols(_In_ const Object& contextObject, 
                         _In_ Object moduleArg,
                         _In_ std::optional<Object> options);

};

//*************************************************
// Main Provider:
//

// ApiProvider:
//
// The singleton provider class which adds "Debugger.Utility.SymbolBuilder.*" as
// new APIs via the data model.
//
class ApiProvider
{
public:

    ApiProvider();
    ~ApiProvider();

    //*************************************************
    // General Factories:
    //

    SymbolSetObject& GetSymbolSetFactory() const { return *m_spSymbolSetFactory.get(); }
    TypesObject& GetTypesFactory() const { return *m_spTypesFactory.get(); }
    DataObject& GetDataFactory() const { return *m_spDataFactory.get(); }
    FunctionsObject& GetFunctionsFactory() const { return *m_spFunctionsFactory.get(); }
    PublicsObject& GetPublicsFactory() const { return *m_spPublicsFactory.get(); }

    //
    // Types:
    //

    BasicTypeObject& GetBasicTypeFactory() const { return *m_spBasicTypeFactory.get(); }
    UdtTypeObject& GetUdtTypeFactory() const { return *m_spUdtTypeFactory.get(); }
    PointerTypeObject& GetPointerTypeFactory() const { return *m_spPointerTypeFactory.get(); }
    ArrayTypeObject& GetArrayTypeFactory() const { return *m_spArrayTypeFactory.get(); }
    TypedefTypeObject& GetTypedefTypeFactory() const { return *m_spTypedefTypeFactory.get(); }
    EnumTypeObject& GetEnumTypeFactory() const { return *m_spEnumTypeFactory.get(); }

    //
    // Data:
    //

    GlobalDataObject& GetGlobalDataFactory() const { return *m_spGlobalDataFactory.get(); }

    //
    // Functions:
    //

    FunctionObject& GetFunctionFactory() const { return *m_spFunctionFactory.get(); }

    //
    // Publics:
    //

    PublicObject& GetPublicFactory() const { return *m_spPublicFactory.get(); }

    //
    // Other Symbols:
    //

    FieldsObject& GetFieldsFactory() const { return *m_spFieldsFactory.get(); }
    FieldObject& GetFieldFactory() const { return *m_spFieldFactory.get(); }
    BaseClassesObject& GetBaseClassesFactory() const { return *m_spBaseClassesFactory.get(); }
    BaseClassObject& GetBaseClassFactory() const { return *m_spBaseClassFactory.get(); }
    EnumerantsObject& GetEnumerantsFactory() const { return *m_spEnumerantsFactory.get(); }
    ParametersObject& GetParametersFactory() const { return *m_spParametersFactory.get(); }
    ParameterObject& GetParameterFactory() const { return *m_spParameterFactory.get(); }
    LocalVariablesObject& GetLocalVariablesFactory() const { return *m_spLocalVariablesFactory.get(); }
    LocalVariableObject& GetLocalVariableFactory() { return *m_spLocalVariableFactory.get(); }
    LiveRangesObject& GetLiveRangesFactory() const { return *m_spLiveRangesFactory.get(); }
    LiveRangeObject& GetLiveRangeFactory() const { return *m_spLiveRangeFactory.get(); }

    // Get():
    //
    // Gets the api provider instance.
    //
    static ApiProvider& Get()
    {
        return *s_pProvider;
    }

private:

    static ApiProvider *s_pProvider;

    //*************************************************
    // General Factories:
    //

    std::unique_ptr<SymbolSetObject> m_spSymbolSetFactory;
    std::unique_ptr<TypesObject> m_spTypesFactory;
    std::unique_ptr<DataObject> m_spDataFactory;
    std::unique_ptr<FunctionsObject> m_spFunctionsFactory;
    std::unique_ptr<PublicsObject> m_spPublicsFactory;

    //
    // Types:
    //

    std::unique_ptr<BasicTypeObject> m_spBasicTypeFactory;
    std::unique_ptr<UdtTypeObject> m_spUdtTypeFactory;
    std::unique_ptr<PointerTypeObject> m_spPointerTypeFactory;
    std::unique_ptr<ArrayTypeObject> m_spArrayTypeFactory;
    std::unique_ptr<TypedefTypeObject> m_spTypedefTypeFactory;
    std::unique_ptr<EnumTypeObject> m_spEnumTypeFactory;

    //
    // Data:
    //

    std::unique_ptr<GlobalDataObject> m_spGlobalDataFactory;

    //
    // Functions:
    //

    std::unique_ptr<FunctionObject> m_spFunctionFactory;

    //
    // Publics:
    //

    std::unique_ptr<PublicObject> m_spPublicFactory;

    //
    // Other Symbols:
    //

    std::unique_ptr<FieldsObject> m_spFieldsFactory;
    std::unique_ptr<FieldObject> m_spFieldFactory;
    std::unique_ptr<BaseClassesObject> m_spBaseClassesFactory;
    std::unique_ptr<BaseClassObject> m_spBaseClassFactory;
    std::unique_ptr<EnumerantsObject> m_spEnumerantsFactory;
    std::unique_ptr<ParametersObject> m_spParametersFactory;
    std::unique_ptr<ParameterObject> m_spParameterFactory;
    std::unique_ptr<LocalVariablesObject> m_spLocalVariablesFactory;
    std::unique_ptr<LocalVariableObject> m_spLocalVariableFactory;
    std::unique_ptr<LiveRangesObject> m_spLiveRangesFactory;
    std::unique_ptr<LiveRangeObject> m_spLiveRangeFactory;

    //*************************************************
    // Extension Points:

    std::unique_ptr<SymbolBuilderNamespace> m_spSymbolBuilderNamespaceExtension;
    std::unique_ptr<ModuleExtension> m_spModuleExtension;

};

} // SymbolBuilder
} // Libraries
} // DataModel
} // Debugger

#endif // __APIPROVIDER_H__
