//**************************************************************************
//
// ApiProvider.cpp
//
// A provider for the data model which extends Debugger.Utility.*
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

#include "SymBuilder.h"
#include "ObjectModel.h"

using namespace Debugger::DataModel::ClientEx;
using namespace Debugger::DataModel::ProviderEx;
using namespace Debugger::TargetComposition::Services::SymbolBuilder;
using namespace Microsoft::WRL;

//*************************************************
// Global State
//
// This is global state required to be a data model provider extension using the
// DbgModelClientEx.h library.
//

Debugger::DataModel::Libraries::SymbolBuilder::ApiProvider *g_pProvider = nullptr;
IDataModelManager *g_pManager = nullptr;
IDebugHost *g_pHost = nullptr;

namespace Debugger::DataModel::ClientEx
{
    IDataModelManager *GetManager() { return g_pManager; }
    IDebugHost *GetHost() { return g_pHost; }
}

namespace Debugger
{
namespace DataModel
{
namespace Libraries
{
namespace SymbolBuilder
{

//*************************************************
// Standard Helpers:
//

// ValueToString():
//
// Performs our string conversion of a constant valued symbol.
//
std::wstring ValueToString(_In_ VARIANT const& val)
{
    wchar_t buf[128];

    switch(val.vt)
    {
        case VT_BOOL:
            return (val.boolVal != VARIANT_FALSE) ? L"true" : L"false";

        case VT_I1:
        {
            swprintf_s(buf, ARRAYSIZE(buf), L"%d", (int)(val.cVal));
            break;
        }

        case VT_I2:
        {
            swprintf_s(buf, ARRAYSIZE(buf), L"%d", (int)(val.iVal));
            break;
        }

        case VT_I4:
        {
            swprintf_s(buf, ARRAYSIZE(buf), L"%d", (int)(val.lVal));
            break;
        }

        case VT_I8:
        {
            swprintf_s(buf, ARRAYSIZE(buf), L"%I64d", val.llVal);
            break;
        }

        case VT_UI1:
        {
            swprintf_s(buf, ARRAYSIZE(buf), L"%ud", (unsigned int)(val.bVal));
            break;
        }

        case VT_UI2:
        {
            swprintf_s(buf, ARRAYSIZE(buf), L"%ud", (unsigned int)(val.uiVal));
            break;
        }

        case VT_UI4:
        {
            swprintf_s(buf, ARRAYSIZE(buf), L"%ud", (unsigned int)(val.ulVal));
            break;
        }

        case VT_UI8:
        {
            swprintf_s(buf, ARRAYSIZE(buf), L"%I64u", val.ullVal);
            break;
        }

        default:
            throw std::runtime_error("illegal constant value");
    }

    return (std::wstring)buf;
}

//*************************************************
// Provider Implementation
//

// GetSymbolBuilderManager():
//
// A helper bridge interface which returns a reference counted pointer to the appropriate symbol builder manager
// for the given host context.
//
// Note that this may throw on failure.
//
void GetSymbolBuilderManager(_In_ const HostContext& ctx,
                             _COM_Outptr_ ISvcSymbolBuilderManager **ppSymbolBuilderManager,
                             _COM_Outptr_opt_ ISvcProcess **ppProcess)
{
    *ppSymbolBuilderManager = nullptr;
    if (ppProcess != nullptr)
    {
        *ppProcess = nullptr;
    }

    //
    // We need to go down a level from the data model into the target composition service container
    // and find our "manager" to deal with things.
    //
    ComPtr<IDebugHostContextTargetComposition> spBridge;
    CheckHr(ctx->QueryInterface(IID_PPV_ARGS(&spBridge)));

    ComPtr<IDebugServiceManager> spServiceManager;
    ComPtr<ISvcProcess> spServiceProcess;
    CheckHr(spBridge->GetServiceManager(&spServiceManager));
    if (ppProcess != nullptr)
    {
        CheckHr(spBridge->GetServiceProcess(&spServiceProcess));
    }

    //
    // If we haven't *YET* injected our manager into *THIS* service container, do so now.
    // We *COULD* have done this at DebugExtensionInitialize on the current container, but that would
    // break multi-process debugging as we only get that notification once.  Since we are ONLY used when
    // an API is called, this is just a convenient place to inject.
    //
    ComPtr<ISvcSymbolBuilderManager> spSymManager;
    if (FAILED(spServiceManager->QueryService(DEBUG_PRIVATE_SERVICE_SYMBOLBUILDER_MANAGER, 
                                              IID_PPV_ARGS(&spSymManager))))
    {
        ComPtr<SymbolBuilderManager> spManager;
        CheckHr(MakeAndInitialize<SymbolBuilderManager>(&spManager));
        CheckHr(spManager->RegisterServices(spServiceManager.Get()));
        spSymManager = std::move(spManager);

        //
        // If we never injected the manager, we never injected the symbol provider.
        //
        ComPtr<SymbolProvider> spProvider;
        CheckHr(MakeAndInitialize<SymbolProvider>(&spProvider));

        //
        // If we call spProvider->RegisterServices(pServiceManager), it will *REPLACE* the symbol provider
        // in the container with ours.  Instead, we will go to an updated interface on the service manager
        // and ask it to aggregate the symbol provider (which means to insert it in a group).
        //
        // Most recent debuggers will support IDebugServiceManager5, but the alternative is shown in terms
        // of handling updated interfaces more generally.
        //
        ComPtr<IDebugServiceManager5> spServiceManager5;
        if (SUCCEEDED(spServiceManager.As(&spServiceManager5)))
        {
            CheckHr(spServiceManager5->AggregateService(DEBUG_SERVICE_SYMBOL_PROVIDER, spProvider.Get()));
        }
        else
        {
            CheckHr(spProvider->RegisterServices(spServiceManager.Get()));
        }
    }

    *ppSymbolBuilderManager = spSymManager.Detach();
    if (ppProcess != nullptr)
    {
        *ppProcess = spServiceProcess.Detach();
    }
}

ApiProvider *ApiProvider::s_pProvider = nullptr;

ApiProvider::ApiProvider()
{
    //
    // The pattern here is that there is a *SINGLE* global ApiProvider constructed when we initialize the
    // extension which makes all of the requisite changes to the object model.  When the singleton ApiProvider
    // is destructed, all of the changes made during construction are reversed.
    //
    // Something is seriously amiss if there is already an ApiProvider at this point!
    //
    if (s_pProvider != nullptr)
    {
        throw std::logic_error("Internal error: ApiProvider singleton is already created");
    }

    s_pProvider = this;

    //*************************************************
    // Initialize our extension points.  These are classes which add new properties or namespaces to existing
    // constructs within the debugger (e.g.: to "Debugger.Utility.*" or to the debugger's notion of a module)
    //
    // When these classes go away, the extensions are removed.
    //

    m_spSymbolBuilderNamespaceExtension = std::make_unique<SymbolBuilderNamespace>();
    m_spModuleExtension = std::make_unique<ModuleExtension>();

    //*************************************************
    // Initialize our typed object models / factories.  These are classes which represent a binding between one
    // of our objects (at the target composition level) and the data model.
    //
    // When these classes go away, the bindings are removed.
    //
    
    //
    // General:
    //

    m_spSymbolSetFactory = std::make_unique<SymbolSetObject>();
    m_spTypesFactory = std::make_unique<TypesObject>();
    m_spDataFactory = std::make_unique<DataObject>();

    //
    // Types:
    //

    m_spBasicTypeFactory = std::make_unique<BasicTypeObject>();
    m_spUdtTypeFactory = std::make_unique<UdtTypeObject>();
    m_spPointerTypeFactory = std::make_unique<PointerTypeObject>();
    m_spArrayTypeFactory = std::make_unique<ArrayTypeObject>();
    m_spTypedefTypeFactory = std::make_unique<TypedefTypeObject>();
    m_spEnumTypeFactory = std::make_unique<EnumTypeObject>();

    //
    // Data:
    //
    
    m_spGlobalDataFactory = std::make_unique<GlobalDataObject>();

    //
    // Other Symbols:
    //

    m_spFieldsFactory = std::make_unique<FieldsObject>();
    m_spFieldFactory = std::make_unique<FieldObject>();
    m_spBaseClassesFactory = std::make_unique<BaseClassesObject>();
    m_spBaseClassFactory = std::make_unique<BaseClassObject>();
    m_spEnumerantsFactory = std::make_unique<EnumerantsObject>();
}

ApiProvider::~ApiProvider()
{
    s_pProvider = nullptr;
}

//*************************************************
// Namespace APIs:
//

Object SymbolBuilderNamespace::CreateSymbols(_In_ const Object& /*contextObject*/,
                                             _In_ Object moduleArg,
                                             _In_ std::optional<Object> options)
{
    ModelObjectKind moduleArgKind = moduleArg.GetKind();

    bool autoImportSymbols = false;
    ULONG64 moduleBase = 0;
    Object moduleObject;
    switch(moduleArgKind)
    {
        case ObjectIntrinsic:
        {
            //
            // There isn't a convenient C++ way to ask whether the intrinsic is a string, number, float,
            // etc...  We can certainly try to cast and catch a throw; however, we'll just go down one level
            // and see if it's unpackable as a 64-bit unsigned before trying the "convert to string"
            //
            VARIANT vtVal;
            if (SUCCEEDED(moduleArg->GetIntrinsicValueAs(VT_UI8, &vtVal)))
            {
                moduleBase = vtVal.ullVal;
            }

            std::wstring moduleName = (std::wstring)moduleArg;

            Object modules = Object::CurrentProcess().KeyValue(L"Modules");
            moduleObject = modules[moduleName];
            break;
        }

        default:
        {
            moduleObject = moduleArg;
            break;
        }
    }

    HostContext moduleContext;
    if (moduleObject != nullptr)
    {
        moduleBase = (ULONG64)moduleObject.KeyValue(L"BaseAddress");
        moduleContext = (HostContext)moduleObject;
    }
    else
    {
        moduleContext = HostContext::Current();
    }

    if (options.has_value())
    {
        Object optionsObj = options.value();
        std::optional<Object> autoImportSymbolsKey = optionsObj.TryGetKeyValue(L"AutoImportSymbols");
        if (autoImportSymbolsKey.has_value())
        {
            autoImportSymbols = (bool)autoImportSymbolsKey.value();
        }
    }

    ComPtr<ISvcSymbolBuilderManager> spSymbolManager;
    ComPtr<ISvcProcess> spProcess;
    GetSymbolBuilderManager(moduleContext, &spSymbolManager, &spProcess);

    ComPtr<SymbolBuilderProcess> spSymbolProcess;
    CheckHr(spSymbolManager->TrackProcess(spProcess.Get(), &spSymbolProcess));

    ComPtr<ISvcModule> spModule;
    CheckHr(spSymbolManager->ModuleBaseToModule(spProcess.Get(), moduleBase, &spModule));

    ULONG64 moduleKey;
    CheckHr(spModule->GetKey(&moduleKey));

    //
    // Once we know what module we're trying to create symbol builder symbols for, check whether we have
    // *ALREADY* done that for this particular module.  You can only do this once.
    //
    ComPtr<SymbolSet> spSymbolSet;
    if (spSymbolProcess->TryGetSymbolsForModule(moduleKey, &spSymbolSet))
    {
        throw std::invalid_argument("module");
    }

    CheckHr(spSymbolProcess->CreateSymbolsForModule(spModule.Get(), moduleKey, &spSymbolSet));

    //
    // If we have been asked to automatically import symbols, set up an appropriate "on demand" importer.
    // It is *NOT* a failure to create the symbol builder set if we cannot set up the importer!
    //
    std::unique_ptr<SymbolImporter> spImporter;
    if (autoImportSymbols)
    {
        //
        // Go ask the debugger through the data model for its symbol path.
        //
        std::wstring symPath = (std::wstring)Object::RootNamespace().KeyValue(L"Debugger")
                                                                    .KeyValue(L"Settings")
                                                                    .KeyValue(L"Symbols")
                                                                    .KeyValue(L"Sympath");

        spImporter.reset(new SymbolImporter_DbgHelp(spSymbolSet.Get(), symPath.c_str()));
        if (SUCCEEDED(spImporter->ConnectToSource()))
        {
            spSymbolSet->SetImporter(std::move(spImporter));
        }
    }

    SymbolSetObject& symbolSetFactory = ApiProvider::Get().GetSymbolSetFactory();
    return symbolSetFactory.CreateInstance(spSymbolSet);
}

//*************************************************
// Object Extensions (Module)
//

Object ModuleExtension::GetSymbolBuilderSymbols(_In_ const Object& moduleObject)
{
    HostContext moduleContext = (HostContext)moduleObject;

    ComPtr<ISvcSymbolBuilderManager> spSymbolManager;
    ComPtr<ISvcProcess> spProcess;
    GetSymbolBuilderManager(moduleContext, &spSymbolManager, &spProcess);

    ComPtr<SymbolBuilderProcess> spSymbolProcess;
    CheckHr(spSymbolManager->TrackProcess(spProcess.Get(), &spSymbolProcess));

    ULONG64 moduleBase = (ULONG64)moduleObject.KeyValue(L"BaseAddress");

    ComPtr<ISvcModule> spModule;
    CheckHr(spSymbolManager->ModuleBaseToModule(spProcess.Get(), moduleBase, &spModule));

    ULONG64 moduleKey;
    CheckHr(spModule->GetKey(&moduleKey));

    ComPtr<SymbolSet> spSymbolSet;
    if (!spSymbolProcess->TryGetSymbolsForModule(moduleKey, &spSymbolSet))
    {
        //
        // Returning "no value" will mean that this property really isn't applicable for this object...  it has.
        // no value.  The debugger's display engine will, by default, not display any properties which have
        // no value.
        //
        return Object::CreateNoValue();
    }

    SymbolSetObject& symbolSetFactory = ApiProvider::Get().GetSymbolSetFactory();
    return symbolSetFactory.CreateInstance(spSymbolSet);
}

//*************************************************
// Symbol Set APIs:
//

Object SymbolSetObject::GetTypes(_In_ const Object& /*symbolSetObject*/,
                                 _In_ ComPtr<SymbolSet>& spSymbolSet)
{
    TypesObject& typesFactory = ApiProvider::Get().GetTypesFactory();
    return typesFactory.CreateInstance(spSymbolSet);
}

Object SymbolSetObject::GetData(_In_ const Object& /*symbolSetObject*/,
                                _In_ ComPtr<SymbolSet>& spSymbolSet)
{
    DataObject& dataFactory = ApiProvider::Get().GetDataFactory();
    return dataFactory.CreateInstance(spSymbolSet);
}

//*************************************************
// General Symbol Helpers:
//

BaseTypeSymbol *SymbolObjectHelpers::UnboxType(_In_ SymbolSet *pSymbolSet, 
                                               _In_ Object typeObject,
                                               _In_ bool allowAutoCreations)

{
    //
    // NOTE: Every raw assignment of pBaseType which holds a raw pointer from a ComPtr<> which will go out of 
    //       scope is perfectly safe *BECAUSE* there is a guarantee that the symbol is held globally by the
    //       symbol set in its index table.  No one could possibly delete it out of that table while this function
    //       is executing.
    //
    BaseTypeSymbol *pBaseType = nullptr;
    if (ApiProvider::Get().GetBasicTypeFactory().IsObjectInstance(typeObject))
    {
        ComPtr<BasicTypeSymbol> spBasicType = ApiProvider::Get().GetBasicTypeFactory().GetStoredInstance(typeObject);
        pBaseType = spBasicType.Get();
    }
    else if (ApiProvider::Get().GetUdtTypeFactory().IsObjectInstance(typeObject))
    {
        ComPtr<UdtTypeSymbol> spUdtType = ApiProvider::Get().GetUdtTypeFactory().GetStoredInstance(typeObject);
        pBaseType = spUdtType.Get();
    }
    else if (ApiProvider::Get().GetPointerTypeFactory().IsObjectInstance(typeObject))
    {
        ComPtr<PointerTypeSymbol> spPointerType = ApiProvider::Get().GetPointerTypeFactory().GetStoredInstance(typeObject);
        pBaseType = spPointerType.Get();
    }
    else if (ApiProvider::Get().GetArrayTypeFactory().IsObjectInstance(typeObject))
    {
        ComPtr<ArrayTypeSymbol> spArrayType = ApiProvider::Get().GetArrayTypeFactory().GetStoredInstance(typeObject);
        pBaseType = spArrayType.Get();
    }
    else if (ApiProvider::Get().GetTypedefTypeFactory().IsObjectInstance(typeObject))
    {
        ComPtr<TypedefTypeSymbol> spTypedefType = ApiProvider::Get().GetTypedefTypeFactory().GetStoredInstance(typeObject);
        pBaseType = spTypedefType.Get();
    }
    else if (ApiProvider::Get().GetEnumTypeFactory().IsObjectInstance(typeObject))
    {
        ComPtr<EnumTypeSymbol> spEnumType = ApiProvider::Get().GetEnumTypeFactory().GetStoredInstance(typeObject);
        pBaseType = spEnumType.Get();
    }
    else
    {
        std::wstring typeName = (std::wstring)typeObject;
        ULONG64 typeId;
        CheckHr(pSymbolSet->FindTypeByName(typeName, &typeId, &pBaseType, allowAutoCreations));
    }

    return pBaseType;
}

Object SymbolObjectHelpers::BoxRelatedType(_In_ BaseSymbol *pSymbol, _In_ ULONG64 typeId)
{
    BaseSymbol *pRelatedSymbol = pSymbol->InternalGetSymbolSet()->InternalGetSymbol(typeId);
    if (pRelatedSymbol == nullptr || pRelatedSymbol->InternalGetKind() != SvcSymbolType)
    {
        throw std::runtime_error("unrecognized type");
    }

    return BoxType(static_cast<BaseTypeSymbol *>(pRelatedSymbol));
}

Object SymbolObjectHelpers::BoxType(_In_ BaseTypeSymbol *pSymbol)
{
    SvcSymbolTypeKind tk = pSymbol->InternalGetTypeKind();

    Object typeObject;
    switch(tk)
    {
        case SvcSymbolTypeIntrinsic:
        {
            BasicTypeSymbol *pBasicSymbol = static_cast<BasicTypeSymbol *>(pSymbol);
            ComPtr<BasicTypeSymbol> spBasicSymbol = pBasicSymbol;
            BasicTypeObject& basicTypeFactory = ApiProvider::Get().GetBasicTypeFactory();
            typeObject = basicTypeFactory.CreateInstance(spBasicSymbol);
            break;
        }

        case SvcSymbolTypeUDT:
        {
            UdtTypeSymbol *pUdtSymbol = static_cast<UdtTypeSymbol *>(pSymbol);
            ComPtr<UdtTypeSymbol> spUdtSymbol = pUdtSymbol;
            UdtTypeObject& udtTypeFactory = ApiProvider::Get().GetUdtTypeFactory();
            typeObject = udtTypeFactory.CreateInstance(spUdtSymbol);
            break;
        }

        case SvcSymbolTypePointer:
        {
            PointerTypeSymbol *pPointerSymbol = static_cast<PointerTypeSymbol *>(pSymbol);
            ComPtr<PointerTypeSymbol> spPointerSymbol = pPointerSymbol;
            PointerTypeObject& pointerTypeFactory = ApiProvider::Get().GetPointerTypeFactory();
            typeObject = pointerTypeFactory.CreateInstance(spPointerSymbol);
            break;
        }

        case SvcSymbolTypeArray:
        {
            ArrayTypeSymbol *pArraySymbol = static_cast<ArrayTypeSymbol *>(pSymbol);
            ComPtr<ArrayTypeSymbol> spArraySymbol = pArraySymbol;
            ArrayTypeObject& arrayTypeFactory = ApiProvider::Get().GetArrayTypeFactory();
            typeObject = arrayTypeFactory.CreateInstance(spArraySymbol);
            break;
        }

        case SvcSymbolTypeTypedef:
        {
            TypedefTypeSymbol *pTypedefSymbol = static_cast<TypedefTypeSymbol *>(pSymbol);
            ComPtr<TypedefTypeSymbol> spTypedefSymbol = pTypedefSymbol;
            TypedefTypeObject& typedefTypeFactory = ApiProvider::Get().GetTypedefTypeFactory();
            typeObject = typedefTypeFactory.CreateInstance(spTypedefSymbol);
            break;
        }

        case SvcSymbolTypeEnum:
        {
            EnumTypeSymbol *pEnumSymbol = static_cast<EnumTypeSymbol *>(pSymbol);
            ComPtr<EnumTypeSymbol> spEnumSymbol = pEnumSymbol;
            EnumTypeObject& enumTypeFactory = ApiProvider::Get().GetEnumTypeFactory();
            typeObject = enumTypeFactory.CreateInstance(spEnumSymbol);
            break;
        }

        default:
            throw std::runtime_error("unrecognized type");
    }

    return typeObject;
}

Object SymbolObjectHelpers::BoxSymbol(_In_ BaseSymbol *pSymbol)
{
    Object symbolObject;
    switch(pSymbol->InternalGetKind())
    {
        case SvcSymbolType:
            return BoxType(static_cast<BaseTypeSymbol *>(pSymbol));

        case SvcSymbolField:
        {
            FieldSymbol *pFieldSymbol = static_cast<FieldSymbol *>(pSymbol);
            ComPtr<FieldSymbol> spFieldSymbol = pFieldSymbol;
            FieldObject& fieldFactory = ApiProvider::Get().GetFieldFactory();
            symbolObject = fieldFactory.CreateInstance(spFieldSymbol);
            break;
        }

        case SvcSymbolBaseClass:
        {
            BaseClassSymbol *pBaseClassSymbol = static_cast<BaseClassSymbol *>(pSymbol);
            ComPtr<BaseClassSymbol> spBaseClassSymbol = pBaseClassSymbol;
            BaseClassObject& baseClassFactory = ApiProvider::Get().GetBaseClassFactory();
            symbolObject = baseClassFactory.CreateInstance(spBaseClassSymbol);
            break;
        }

        default:
            throw std::runtime_error("unrecognized symbol");
    }

    return symbolObject;
}

//*************************************************
// Types APIs:

void TypesObject::AddBasicCTypes(_In_ const Object& /*typesObjec*/, _In_ ComPtr<SymbolSet>& spSymbolSet)
{
    SymbolSet *pSymbolSet = spSymbolSet.Get();
    CheckHr(pSymbolSet->AddBasicCTypes());
}

Object TypesObject::Create(_In_ const Object& /*typesObject*/, 
                           _In_ ComPtr<SymbolSet>& spSymbolSet,
                           _In_ std::wstring typeName,
                           _In_ std::optional<std::wstring> qualifiedTypeName)
{
    SymbolSet *pSymbolSet = spSymbolSet.Get();

    PCWSTR pwszTypeName = typeName.c_str();
    PCWSTR pwszQualifiedName = (qualifiedTypeName.has_value() ? qualifiedTypeName.value().c_str() : nullptr);

    ComPtr<UdtTypeSymbol> spUdt;
    CheckHr(MakeAndInitialize<UdtTypeSymbol>(&spUdt, pSymbolSet, 0, pwszTypeName, pwszQualifiedName));

    UdtTypeObject& udtTypeFactory = ApiProvider::Get().GetUdtTypeFactory();
    return udtTypeFactory.CreateInstance(spUdt);
}

Object TypesObject::CreatePointer(_In_ const Object& /*typesObject*/,
                                  _In_ ComPtr<SymbolSet>& spSymbolSet,
                                  _In_ Object pointedToType)
{
    BaseTypeSymbol *pPointedToType = UnboxType(spSymbolSet.Get(), pointedToType);

    ComPtr<PointerTypeSymbol> spPointer;
    CheckHr(MakeAndInitialize<PointerTypeSymbol>(&spPointer, 
                                                 spSymbolSet.Get(), 
                                                 pPointedToType->InternalGetId(), 
                                                 SvcSymbolPointerStandard /* @TODO: */));

    PointerTypeObject &pointerTypeFactory = ApiProvider::Get().GetPointerTypeFactory();
    return pointerTypeFactory.CreateInstance(spPointer);
}

Object TypesObject::CreateArray(_In_ const Object& /*typesObject*/,
                                _In_ ComPtr<SymbolSet>& spSymbolSet,
                                _In_ Object arrayOfType,
                                _In_ ULONG64 arrayDim)
{
    BaseTypeSymbol *pArrayOfType = UnboxType(spSymbolSet.Get(), arrayOfType);

    ComPtr<ArrayTypeSymbol> spArray;
    CheckHr(MakeAndInitialize<ArrayTypeSymbol>(&spArray,
                                               spSymbolSet.Get(),
                                               pArrayOfType->InternalGetId(),
                                               arrayDim));

    ArrayTypeObject &arrayTypeFactory = ApiProvider::Get().GetArrayTypeFactory();
    return arrayTypeFactory.CreateInstance(spArray);
}

Object TypesObject::CreateTypedef(_In_ const Object& /*typesObject*/,
                                  _In_ ComPtr<SymbolSet>& spSymbolSet,
                                  _In_ std::wstring typeName,
                                  _In_ Object typedefOfType,
                                  _In_ std::optional<std::wstring> qualifiedTypeName)
{
    BaseTypeSymbol *pTypedefOfType = UnboxType(spSymbolSet.Get(), typedefOfType);

    PCWSTR pwszTypeName = typeName.c_str();
    PCWSTR pwszQualifiedName = (qualifiedTypeName.has_value() ? qualifiedTypeName.value().c_str() : nullptr);

    ComPtr<TypedefTypeSymbol> spTypedef;
    CheckHr(MakeAndInitialize<TypedefTypeSymbol>(&spTypedef, 
                                                 spSymbolSet.Get(), 
                                                 pTypedefOfType->InternalGetId(),
                                                 0, 
                                                 pwszTypeName,
                                                 pwszQualifiedName));

    TypedefTypeObject &typedefTypeFactory = ApiProvider::Get().GetTypedefTypeFactory();
    return typedefTypeFactory.CreateInstance(spTypedef);
}

Object TypesObject::CreateEnum(_In_ const Object& /*typesObject*/,
                               _In_ ComPtr<SymbolSet>& spSymbolSet,
                               _In_ std::wstring typeName,
                               _In_ std::optional<Object> enumBasicType,
                               _In_ std::optional<std::wstring> qualifiedTypeName)
{
    //
    // If there is no value for the basic type (it's not provided), default to "int" which is the
    // standard C default.
    //
    ULONG64 basicTypeId;
    if (enumBasicType.has_value())
    {
        BaseTypeSymbol *pEnumBasicType = UnboxType(spSymbolSet.Get(), enumBasicType.value());
        basicTypeId = pEnumBasicType->InternalGetId();
    }
    else
    {
        CheckHr(spSymbolSet->FindTypeByName(L"int", &basicTypeId, nullptr, false));
    }

    PCWSTR pwszTypeName = typeName.c_str();
    PCWSTR pwszQualifiedName = (qualifiedTypeName.has_value() ? qualifiedTypeName.value().c_str() : nullptr);

    ComPtr<EnumTypeSymbol> spEnum;
    CheckHr(MakeAndInitialize<EnumTypeSymbol>(&spEnum, 
                                              spSymbolSet.Get(), 
                                              basicTypeId,
                                              0, 
                                              pwszTypeName,
                                              pwszQualifiedName));

    EnumTypeObject &enumTypeFactory = ApiProvider::Get().GetEnumTypeFactory();
    return enumTypeFactory.CreateInstance(spEnum);
}

std::experimental::generator<Object> TypesObject::GetIterator(_In_ const Object& /*typesObject*/,
                                                              _In_ ComPtr<SymbolSet>& spSymbolSet)
{
    //
    // We must take **GREAT CARE** with what we touch and how we iterate.  After a co_yield, the state
    // of things may have drastically changed.  We must refetch things and only rely upon positional
    // counters!
    //
    size_t cur = 0;
    for(;;)
    {
        auto&& globalSymbols = spSymbolSet->InternalGetGlobalSymbols();
        if (cur >= globalSymbols.size())
        {
            break;
        }

        ULONG64 nextGlobal = globalSymbols[cur];
        ++cur;

        BaseSymbol *pNextSymbol = spSymbolSet->InternalGetSymbol(nextGlobal);
        if (pNextSymbol->InternalGetKind() != SvcSymbolType)
        {
            continue;
        }

        BaseTypeSymbol *pNextType = static_cast<BaseTypeSymbol *>(pNextSymbol);
        Object typeObject = BoxType(pNextType);
        co_yield typeObject;
    }
}

//*************************************************
// Base Symbols API:
//

template<typename TSym>
Object BaseSymbolObject<TSym>::GetName(_In_ const Object& /*symbolObject*/, _In_ ComPtr<TSym>& spSymbol)
{
    if (spSymbol->InternalGetName().empty())
    {
        return Object::CreateNoValue();
    }

    return spSymbol->InternalGetName();
}

template<typename TSym>
Object BaseSymbolObject<TSym>::GetQualifiedName(_In_ const Object& /*symbolObject*/, _In_ ComPtr<TSym>& spSymbol)
{
    if (spSymbol->InternalGetQualifiedName().empty())
    {
        return Object::CreateNoValue();
    }

    return spSymbol->InternalGetQualifiedName();
}

template<typename TSym>
Object BaseSymbolObject<TSym>::GetParent(_In_ const Object& /*symbolObject*/, _In_ ComPtr<TSym>& spSymbol)
{
    BaseSymbol *pParentSymbol = spSymbol->InternalGetSymbolSet()->InternalGetSymbol(spSymbol->InternalGetParentId());
    if (pParentSymbol == nullptr)
    {
        return Object::CreateNoValue();
    }

    return BoxSymbol(pParentSymbol);
}

//*************************************************
// Base Types APIs:
//

template<typename TType>
ULONG64 BaseTypeObject<TType>::GetSize(_In_ const Object& /*typeObject*/, _In_ ComPtr<TType>& spTypeSymbol)
{
    return spTypeSymbol->InternalGetTypeSize();
}

template<typename TType>
ULONG64 BaseTypeObject<TType>::GetAlignment(_In_ const Object& /*typeObject*/, _In_ ComPtr<TType>& spTypeSymbol)
{
    return spTypeSymbol->InternalGetTypeAlignment();
}

template<typename TType>
std::wstring BaseTypeObject<TType>::ToString(_In_ const Object& /*typeObject*/,
                                             _In_ ComPtr<TType>& spTypeSymbol,
                                             _In_ const Metadata& /*metadata*/)
{
    std::wstring const& name = spTypeSymbol->InternalGetQualifiedName();

    wchar_t buf[512];
    swprintf_s(buf, ARRAYSIZE(buf), L"%s: %s ( size = %d, align = %d )",
               m_pwszConvTag,
               name.empty() ? L"<Unknown>" : name.c_str(),
               (ULONG)spTypeSymbol->InternalGetTypeSize(),
               (ULONG)spTypeSymbol->InternalGetTypeAlignment());

    return (std::wstring)buf;
}

template<typename TType>
void BaseTypeObject<TType>::Delete(_In_ const Object& /*typeObject*/,
                                   _In_ ComPtr<TType>& spTypeSymbol)
{
    CheckHr(spTypeSymbol->Delete());
}

//*************************************************
// UDT APIs:
//

Object UdtTypeObject::GetBaseClasses(_In_ const Object& /*typeObject*/,
                                     _In_ ComPtr<UdtTypeSymbol>& spUdtTypeSymbol)
{
    BaseClassesObject& baseClassesFactory = ApiProvider::Get().GetBaseClassesFactory();
    return baseClassesFactory.CreateInstance(spUdtTypeSymbol);
}

Object UdtTypeObject::GetFields(_In_ const Object& /*typeObject*/,
                                _In_ ComPtr<UdtTypeSymbol>& spUdtTypeSymbol)
{
    FieldsObject& fieldsFactory = ApiProvider::Get().GetFieldsFactory();
    return fieldsFactory.CreateInstance(spUdtTypeSymbol);
}

//*************************************************
// Pointer APIs:
//

Object PointerTypeObject::GetBaseType(_In_ const Object& /*pointerTypeObject*/,
                                      _In_ ComPtr<PointerTypeSymbol>& spPointerTypeSymbol)
{
    return BoxRelatedType(spPointerTypeSymbol.Get(), spPointerTypeSymbol->InternalGetPointerToTypeId());
}

//*************************************************
// Array APIs:
//

Object ArrayTypeObject::GetBaseType(_In_ const Object& /*arrayTypeObject*/,
                                    _In_ ComPtr<ArrayTypeSymbol>& spArrayTypeSymbol)
{
    return BoxRelatedType(spArrayTypeSymbol.Get(), spArrayTypeSymbol->InternalGetArrayOfTypeId());
}

ULONG64 ArrayTypeObject::GetArraySize(_In_ const Object& /*arrayTypeObject*/, 
                                      _In_ ComPtr<ArrayTypeSymbol>& spArraySymbol)
{
    return spArraySymbol->InternalGetArraySize();
}

//*************************************************
// Typedef APIs:
//

Object TypedefTypeObject::GetBaseType(_In_ const Object& /*typedefTypeObject*/,
                                      _In_ ComPtr<TypedefTypeSymbol>& spTypedefTypeSymbol)
{
    return BoxRelatedType(spTypedefTypeSymbol.Get(), spTypedefTypeSymbol->InternalGetTypedefOfTypeId());
}

//*************************************************
// Enum APIs:
//

Object EnumTypeObject::GetBaseType(_In_ const Object& /*enumTypeObject*/,
                                   _In_ ComPtr<EnumTypeSymbol>& spEnumTypeSymbol)
{
    return BoxRelatedType(spEnumTypeSymbol.Get(), spEnumTypeSymbol->InternalGetEnumBasicTypeId());
}

Object EnumTypeObject::GetEnumerants(_In_ const Object& /*enumObject*/,
                                     _In_ ComPtr<EnumTypeSymbol>& spEnumTypeSymbol)
{
    EnumerantsObject& enumerantsFactory = ApiProvider::Get().GetEnumerantsFactory();
    return enumerantsFactory.CreateInstance(spEnumTypeSymbol);
}

//*************************************************
// Fields APIs:
//

std::experimental::generator<Object> FieldsObject::GetIterator(_In_ const Object& /*fieldsObject*/,
                                                               _In_ ComPtr<UdtTypeSymbol>& spUdtTypeSymbol)
{
    //
    // We must take **GREAT CARE** with what we touch and how we iterate.  After a co_yield, the state
    // of things may have drastically changed.  We must refetch things and only rely upon positional
    // counters!
    //
    size_t cur = 0;
    for(;;)
    {
        auto&& children = spUdtTypeSymbol->InternalGetChildren();
        if (cur >= children.size())
        {
            break;
        }

        ULONG64 nextChild = children[cur];
        ++cur;

        BaseSymbol *pNextSymbol = spUdtTypeSymbol->InternalGetSymbolSet()->InternalGetSymbol(nextChild);
        if (pNextSymbol->InternalGetKind() != SvcSymbolField)
        {
            continue;
        }

        FieldSymbol *pNextField = static_cast<FieldSymbol *>(pNextSymbol);
        ComPtr<FieldSymbol> spNextField = pNextField;
        FieldObject& fieldFactory = ApiProvider::Get().GetFieldFactory();
        Object fieldObj = fieldFactory.CreateInstance(spNextField);
        co_yield fieldObj;
    }
}

Object FieldsObject::Add(_In_ const Object& /*fieldsObject*/,
                         _In_ ComPtr<UdtTypeSymbol>& spUdtTypeSymbol,
                         _In_ std::wstring fieldName,
                         _In_ Object fieldType,
                         _In_ std::optional<ULONG64> fieldOffset)
{
    SymbolSet *pSymbolSet = spUdtTypeSymbol->InternalGetSymbolSet();

    ULONG64 fieldOffsetToUse = fieldOffset.value_or(UdtPositionalSymbol::AutomaticAppendLayout);
    ULONG64 fieldTypeId = 0;

    UdtTypeObject& udtTypeFactory = ApiProvider::Get().GetUdtTypeFactory();
    if (udtTypeFactory.IsObjectInstance(fieldType))
    {
        ComPtr<UdtTypeSymbol> spFieldType_Udt = udtTypeFactory.GetStoredInstance(fieldType);
        fieldTypeId = spFieldType_Udt->InternalGetId();
    }
    else
    {
        std::wstring fieldTypeName = (std::wstring)fieldType;
        fieldTypeId = pSymbolSet->InternalGetSymbolIdByName(fieldTypeName);
    }

    BaseTypeSymbol *pFieldType = UnboxType(pSymbolSet, fieldType);

    ComPtr<FieldSymbol> spField;
    CheckHr(MakeAndInitialize<FieldSymbol>(&spField, 
                                          pSymbolSet,
                                          spUdtTypeSymbol->InternalGetId(),
                                          fieldOffsetToUse,
                                          pFieldType->InternalGetId(),
                                          fieldName.c_str()));

    FieldObject& fieldFactory = ApiProvider::Get().GetFieldFactory();
    return fieldFactory.CreateInstance(spField);
}

//*************************************************
// Enumerants APIs:
//

std::experimental::generator<Object> EnumerantsObject::GetIterator(_In_ const Object& /*enumerantsObject*/,
                                                                   _In_ ComPtr<EnumTypeSymbol>& spEnumTypeSymbol)
{
    //
    // We must take **GREAT CARE** with what we touch and how we iterate.  After a co_yield, the state
    // of things may have drastically changed.  We must refetch things and only rely upon positional
    // counters!
    //
    size_t cur = 0;
    for(;;)
    {
        auto&& children = spEnumTypeSymbol->InternalGetChildren();
        if (cur >= children.size())
        {
            break;
        }

        ULONG64 nextChild = children[cur];
        ++cur;

        BaseSymbol *pNextSymbol = spEnumTypeSymbol->InternalGetSymbolSet()->InternalGetSymbol(nextChild);
        if (pNextSymbol->InternalGetKind() != SvcSymbolField)
        {
            continue;
        }

        FieldSymbol *pNextField = static_cast<FieldSymbol *>(pNextSymbol);
        ComPtr<FieldSymbol> spNextField = pNextField;
        FieldObject& fieldFactory = ApiProvider::Get().GetFieldFactory();
        Object fieldObj = fieldFactory.CreateInstance(spNextField);
        co_yield fieldObj;
    }
}

Object EnumerantsObject::Add(_In_ const Object& /*enumObject*/,
                             _In_ ComPtr<EnumTypeSymbol>& spEnumTypeSymbol,
                             _In_ std::wstring enumerantName,
                             _In_ std::optional<Object> enumerantValue)
{
    SymbolSet *pSymbolSet = spEnumTypeSymbol->InternalGetSymbolSet();

    //
    // We know the enum's packing type as a VARTYPE.  Just ask the underlying data model to convert the
    // value to our packing.  If it can't be done (because it's an overflow, etc...) just throw.
    //
    // NOTE: Because of the limited variant support, we do *NOT* need to VariantInit/VariantClear/etc...
    //
    // If there is no value, consider it an auto-increment enumerant (ala an unvalued "C enum" enumerant)
    //
    VARIANT vtEnumerantValue;
    vtEnumerantValue.vt = VT_EMPTY;

    if (enumerantValue.has_value())
    {
        Object val = enumerantValue.value();
        CheckHr(val->GetIntrinsicValueAs(spEnumTypeSymbol->InternalGetEnumValuePacking(), &vtEnumerantValue));
    }

    ComPtr<FieldSymbol> spField;
    CheckHr(MakeAndInitialize<FieldSymbol>(&spField, 
                                          pSymbolSet,
                                          spEnumTypeSymbol->InternalGetId(),
                                          0,
                                          &vtEnumerantValue,
                                          enumerantName.c_str()));

    FieldObject& fieldFactory = ApiProvider::Get().GetFieldFactory();
    return fieldFactory.CreateInstance(spField);
}

//*************************************************
// Field APIs:
//

std::wstring FieldObject::ToString(_In_ const Object& /*fieldObject*/,
                                   _In_ ComPtr<FieldSymbol>& spFieldSymbol,
                                   _In_ const Metadata& /*metadata*/)
{
    wchar_t buf[512];

    std::wstring const& fieldName = spFieldSymbol->InternalGetName();

    ULONG64 fieldTypeId = spFieldSymbol->InternalGetSymbolTypeId();

    if (fieldTypeId == 0)
    {
        //
        // The only way this is legal is if it is a constant valued enumerant!
        //
        std::wstring value = ValueToString(spFieldSymbol->InternalGetSymbolValue());

        swprintf_s(buf, ARRAYSIZE(buf), L"Enumerant: %s (value = %s)",
                   fieldName.empty() ? L"<Unknown>" : fieldName.c_str(),
                   value.c_str());
    }
    else if (spFieldSymbol->InternalIsConstantValue())
    {
        BaseSymbol *pFieldTypeSymbol = spFieldSymbol->InternalGetSymbolSet()->InternalGetSymbol(fieldTypeId);
        std::wstring const& fieldTypeName = pFieldTypeSymbol->InternalGetQualifiedName();
        std::wstring value = ValueToString(spFieldSymbol->InternalGetSymbolValue());

        swprintf_s(buf, ARRAYSIZE(buf), L"Field: %s (type = '%s', value = %s )",
                   fieldName.empty() ? L"<Unknown>" : fieldName.c_str(),
                   fieldTypeName.empty() ? L"<Unknown>" : fieldTypeName.c_str(),
                   value.c_str());

    }
    else
    {
        BaseSymbol *pFieldTypeSymbol = spFieldSymbol->InternalGetSymbolSet()->InternalGetSymbol(fieldTypeId);
        std::wstring const& fieldTypeName = pFieldTypeSymbol->InternalGetQualifiedName();

        swprintf_s(buf, ARRAYSIZE(buf), L"Field: %s ( type = '%s', offset = %d )",
                   fieldName.empty() ? L"<Unknown>" : fieldName.c_str(),
                   fieldTypeName.empty() ? L"<Unknown>" : fieldTypeName.c_str(),
                   (ULONG)spFieldSymbol->InternalGetActualSymbolOffset());
    }

    return (std::wstring)buf;
}

void FieldObject::Delete(_In_ const Object& /*fieldObject*/,
                         _In_ ComPtr<FieldSymbol>& spFieldSymbol)
{
    CheckHr(spFieldSymbol->Delete());
}

void FieldObject::MoveBefore(_In_ const Object& /*fieldObject*/, _In_ ComPtr<FieldSymbol>& spFieldSymbol,
                             _In_ Object beforeObj)
{
    ULONG64 pos = 0;

    //
    // The 'beforeObj' may be legitimately another field *OR* it may be a positional index of the field
    // within the list of children.  Handle both.
    //
    FieldObject& fieldFactory = ApiProvider::Get().GetFieldFactory();
    if (fieldFactory.IsObjectInstance(beforeObj))
    {
        ComPtr<FieldSymbol> spFieldSymbol = fieldFactory.GetStoredInstance(beforeObj);

        SymbolSet *pSymbolSet = spFieldSymbol->InternalGetSymbolSet();
        BaseSymbol *pParentSymbol = pSymbolSet->InternalGetSymbol(spFieldSymbol->InternalGetParentId());
        if (pParentSymbol == nullptr)
        {
            throw std::runtime_error("cannot rearrange an orphan field");
        }

        CheckHr(pParentSymbol->GetChildPosition(spFieldSymbol->InternalGetId(), &pos));
    }
    else
    {
        pos = (ULONG64)beforeObj;
    }

    CheckHr(spFieldSymbol->MoveToBefore(pos));
}

std::optional<bool> FieldObject::GetIsAutomaticLayout(_In_ const Object& /*fieldObject*/, _In_ ComPtr<FieldSymbol>& spFieldSymbol)
{
    //
    // This does not apply to constant valued fields which are not enumerants.
    //
    std::optional<bool> result;
    if (spFieldSymbol->InternalIsConstantValue() && !spFieldSymbol->InternalIsEnumerant())
    {
        return result;
    }

    result = (spFieldSymbol->InternalIsAutomaticLayout() || spFieldSymbol->InternalIsIncreasingConstant());
    return result;
}

void FieldObject::SetIsAutomaticLayout(_In_ const Object& /*fieldObject*/,
                                       _In_ ComPtr<FieldSymbol>& spFieldSymbol,
                                       _In_ bool isAutomaticLayout)
{
    //
    // This does not apply to constant valued fields which are not enumerants.
    //
    if (spFieldSymbol->InternalIsConstantValue() && !spFieldSymbol->InternalIsEnumerant())
    {
        throw std::runtime_error("cannot change layout of constant valued field");
    }

    switch(spFieldSymbol->InternalGetSymbolOffset())
    {
        default:
        case UdtPositionalSymbol::AutomaticAppendLayout:
        {
            ULONG64 actualOffset = spFieldSymbol->InternalGetActualSymbolOffset();
            CheckHr(spFieldSymbol->InternalSetSymbolOffset(isAutomaticLayout ? UdtPositionalSymbol::AutomaticAppendLayout
                                                                             : actualOffset));
            break;
        }

        case UdtPositionalSymbol::ConstantValue:
        case UdtPositionalSymbol::AutomaticIncreaseConstantValue:
        {
            CheckHr(spFieldSymbol->InternalSetSymbolOffset(isAutomaticLayout ? UdtPositionalSymbol::AutomaticIncreaseConstantValue
                                                                             : UdtPositionalSymbol::ConstantValue));
            break;
        }
    }
}

std::optional<ULONG64> FieldObject::GetOffset(_In_ const Object& /*fieldObject*/, _In_ ComPtr<FieldSymbol>& spFieldSymbol)
{
    std::optional<ULONG64> result;
    
    if (!spFieldSymbol->InternalIsConstantValue())
    {
        result = spFieldSymbol->InternalGetActualSymbolOffset();
    }
    return result;
}

void FieldObject::SetOffset(_In_ const Object& /*fieldObject*/, _In_ ComPtr<FieldSymbol>& spFieldSymbol,
                            _In_ ULONG64 fieldOffset)
{
    if (spFieldSymbol->InternalIsAutomaticLayout())
    {
        throw std::runtime_error("cannot set field offset of a field which is automatic layout");
    }
    else if (spFieldSymbol->InternalIsConstantValue())
    {
        throw std::runtime_error("cannot set field offset of a field which is constnat value");
    }

    CheckHr(spFieldSymbol->InternalSetSymbolOffset(fieldOffset));
}

Object FieldObject::GetType(_In_ const Object& /*fieldObject*/, _In_ ComPtr<FieldSymbol>& spFieldSymbol)
{
    if (spFieldSymbol->InternalGetSymbolTypeId() == 0)
    {
        return Object::CreateNoValue();
    }
    
    return BoxRelatedType(spFieldSymbol.Get(), spFieldSymbol->InternalGetSymbolTypeId());
}

void FieldObject::SetType(_In_ const Object& /*fieldObject*/, _In_ ComPtr<FieldSymbol>& spFieldSymbol,
                          _In_ Object fieldType)
{
    if (spFieldSymbol->InternalGetSymbolTypeId() == 0)
    {
        throw std::runtime_error("cannot set explicit type of an enumerant");
    }

    BaseTypeSymbol *pNewFieldType = UnboxType(spFieldSymbol->InternalGetSymbolSet(),
                                              fieldType);

    CheckHr(spFieldSymbol->InternalSetSymbolTypeId(pNewFieldType->InternalGetId()));
}

//*************************************************
// Base Classes APIs:
//

std::experimental::generator<Object> BaseClassesObject::GetIterator(_In_ const Object& /*baseClassesObject*/,
                                                                    _In_ ComPtr<UdtTypeSymbol>& spUdtTypeSymbol)
{
    //
    // We must take **GREAT CARE** with what we touch and how we iterate.  After a co_yield, the state
    // of things may have drastically changed.  We must refetch things and only rely upon positional
    // counters!
    //
    size_t cur = 0;
    for(;;)
    {
        auto&& children = spUdtTypeSymbol->InternalGetChildren();
        if (cur >= children.size())
        {
            break;
        }

        ULONG64 nextChild = children[cur];
        ++cur;

        BaseSymbol *pNextSymbol = spUdtTypeSymbol->InternalGetSymbolSet()->InternalGetSymbol(nextChild);
        if (pNextSymbol->InternalGetKind() != SvcSymbolBaseClass)
        {
            continue;
        }

        BaseClassSymbol *pNextBaseClass = static_cast<BaseClassSymbol *>(pNextSymbol);
        ComPtr<BaseClassSymbol> spNextBaseClass = pNextBaseClass;
        BaseClassObject& baseClassFactory = ApiProvider::Get().GetBaseClassFactory();
        Object baseClassObj = baseClassFactory.CreateInstance(spNextBaseClass);
        co_yield baseClassObj;
    }
}

Object BaseClassesObject::Add(_In_ const Object& /*fieldsObject*/,
                              _In_ ComPtr<UdtTypeSymbol>& spUdtTypeSymbol,
                              _In_ Object baseClassType,
                              _In_ std::optional<ULONG64> baseClassOffset)
{
    SymbolSet *pSymbolSet = spUdtTypeSymbol->InternalGetSymbolSet();

    ULONG64 baseClassOffsetToUse = baseClassOffset.value_or(UdtPositionalSymbol::AutomaticAppendLayout);
    ULONG64 baseClassTypeId = 0;

    UdtTypeObject& udtTypeFactory = ApiProvider::Get().GetUdtTypeFactory();
    if (udtTypeFactory.IsObjectInstance(baseClassType))
    {
        ComPtr<UdtTypeSymbol> spBaseClassType_Udt = udtTypeFactory.GetStoredInstance(baseClassType);
        baseClassTypeId = spBaseClassType_Udt->InternalGetId();
    }
    else
    {
        std::wstring baseClassTypeName = (std::wstring)baseClassType;
        baseClassTypeId = pSymbolSet->InternalGetSymbolIdByName(baseClassTypeName);
    }

    BaseSymbol *pBaseClassTypeSymbol = pSymbolSet->InternalGetSymbol(baseClassTypeId);
    if (pBaseClassTypeSymbol == nullptr || pBaseClassTypeSymbol->InternalGetKind() != SvcSymbolType)
    {
        throw std::invalid_argument("baseClassType");
    }

    BaseTypeSymbol *pBaseClassType = static_cast<BaseTypeSymbol *>(pBaseClassTypeSymbol);
    if (pBaseClassType->InternalGetTypeKind() != SvcSymbolTypeUDT)
    {
        throw std::invalid_argument("baseClassType");
    }

    ComPtr<BaseClassSymbol> spBaseClass;
    CheckHr(MakeAndInitialize<BaseClassSymbol>(&spBaseClass, 
                                               pSymbolSet,
                                               spUdtTypeSymbol->InternalGetId(),
                                               baseClassOffsetToUse,
                                               baseClassTypeId));

    BaseClassObject& baseClassFactory = ApiProvider::Get().GetBaseClassFactory();
    return baseClassFactory.CreateInstance(spBaseClass);
}

//*************************************************
// Base Class APIs:
//

std::wstring BaseClassObject::ToString(_In_ const Object& /*baseClassObject*/,
                                       _In_ ComPtr<BaseClassSymbol>& spBaseClassSymbol,
                                       _In_ const Metadata& /*metadata*/)
{
    ULONG64 baseClassTypeId = spBaseClassSymbol->InternalGetSymbolTypeId();
    BaseSymbol *pBaseClassTypeSymbol = spBaseClassSymbol->InternalGetSymbolSet()->InternalGetSymbol(baseClassTypeId);

    std::wstring const& baseClassTypeName = pBaseClassTypeSymbol->InternalGetQualifiedName();

    wchar_t buf[512];
    swprintf_s(buf, ARRAYSIZE(buf), L"Base Class: ( type = '%s', offset = %d )",
               baseClassTypeName.empty() ? L"<Unknown>" : baseClassTypeName.c_str(),
               (ULONG)spBaseClassSymbol->InternalGetActualSymbolOffset());

    return (std::wstring)buf;
}

void BaseClassObject::Delete(_In_ const Object& /*baseClassObject*/,
                             _In_ ComPtr<BaseClassSymbol>& spBaseClassSymbol)
{
    CheckHr(spBaseClassSymbol->Delete());
}

void BaseClassObject::MoveBefore(_In_ const Object& /*baseClassObject*/, 
                                 _In_ ComPtr<BaseClassSymbol>& spBaseClassSymbol,
                                 _In_ Object beforeObj)
{
    ULONG64 pos = 0;

    //
    // The 'beforeObj' may be legitimately another base class *OR* it may be a positional index of the base class
    // within the list of children.  Handle both.
    //
    BaseClassObject& baseClassFactory = ApiProvider::Get().GetBaseClassFactory();
    if (baseClassFactory.IsObjectInstance(beforeObj))
    {
        ComPtr<BaseClassSymbol> spBaseClassSymbol = baseClassFactory.GetStoredInstance(beforeObj);

        SymbolSet *pSymbolSet = spBaseClassSymbol->InternalGetSymbolSet();
        BaseSymbol *pParentSymbol = pSymbolSet->InternalGetSymbol(spBaseClassSymbol->InternalGetParentId());
        if (pParentSymbol == nullptr)
        {
            throw std::runtime_error("cannot rearrange an orphan base class");
        }

        CheckHr(pParentSymbol->GetChildPosition(spBaseClassSymbol->InternalGetId(), &pos));
    }
    else
    {
        pos = (ULONG64)beforeObj;
    }

    CheckHr(spBaseClassSymbol->MoveToBefore(pos));
}

bool BaseClassObject::GetIsAutomaticLayout(_In_ const Object& /*baseClassObject*/, 
                                           _In_ ComPtr<BaseClassSymbol>& spBaseClassSymbol)
{
    ULONG64 symOffset = spBaseClassSymbol->InternalGetSymbolOffset();
    return (symOffset == UdtPositionalSymbol::AutomaticAppendLayout);
}

void BaseClassObject::SetIsAutomaticLayout(_In_ const Object& /*baseClassObject*/,
                                           _In_ ComPtr<BaseClassSymbol>& spBaseClassSymbol,
                                           _In_ bool isAutomaticLayout)
{
    ULONG64 actualOffset = spBaseClassSymbol->InternalGetActualSymbolOffset();
    CheckHr(spBaseClassSymbol->InternalSetSymbolOffset(isAutomaticLayout ? UdtPositionalSymbol::AutomaticAppendLayout
                                                                         : actualOffset));
}

ULONG64 BaseClassObject::GetOffset(_In_ const Object& /*baseClassObject*/, 
                                   _In_ ComPtr<BaseClassSymbol>& spBaseClassSymbol)
{
    return spBaseClassSymbol->InternalGetActualSymbolOffset();
}

void BaseClassObject::SetOffset(_In_ const Object& /*baseClassObject*/, 
                                _In_ ComPtr<BaseClassSymbol>& spBaseClassSymbol,
                                _In_ ULONG64 baseClassOffset)
{
    if (spBaseClassSymbol->InternalGetSymbolOffset() == UdtPositionalSymbol::AutomaticAppendLayout)
    {
        throw std::runtime_error("cannot set base class offset of a base class which is automatic layout");
    }

    CheckHr(spBaseClassSymbol->InternalSetSymbolOffset(baseClassOffset));
}

Object BaseClassObject::GetType(_In_ const Object& /*baseClassObject*/, 
                                _In_ ComPtr<BaseClassSymbol>& spBaseClassSymbol)
{
    return BoxRelatedType(spBaseClassSymbol.Get(), spBaseClassSymbol->InternalGetSymbolTypeId());
}

void BaseClassObject::SetType(_In_ const Object& /*baseClassObject*/, 
                              _In_ ComPtr<BaseClassSymbol>& spBaseClassSymbol,
                              _In_ Object baseClassType)
{
    BaseTypeSymbol *pNewBaseClassType = UnboxType(spBaseClassSymbol->InternalGetSymbolSet(),
                                                  baseClassType);

    CheckHr(spBaseClassSymbol->InternalSetSymbolTypeId(pNewBaseClassType->InternalGetId()));
}

//*************************************************
// Data APIs:
//

Object DataObject::CreateGlobal(_In_ const Object& /*dataObject*/, 
                                _In_ ComPtr<SymbolSet>& spSymbolSet,
                                _In_ std::wstring dataName,
                                _In_ Object dataType,
                                _In_ ULONG64 dataOffset,
                                _In_ std::optional<std::wstring> qualifiedDataName)
{
    SymbolSet *pSymbolSet = spSymbolSet.Get();
    BaseTypeSymbol *pDataType = UnboxType(spSymbolSet.Get(), dataType);

    PCWSTR pwszName = dataName.c_str();
    PCWSTR pwszQualifiedName = (qualifiedDataName.has_value() ? qualifiedDataName.value().c_str() : nullptr);

    ComPtr<GlobalDataSymbol> spGlobalData;
    CheckHr(MakeAndInitialize<GlobalDataSymbol>(&spGlobalData, 
                                                pSymbolSet,
                                                0,
                                                dataOffset,
                                                pDataType->InternalGetId(),
                                                pwszName,
                                                pwszQualifiedName));

    GlobalDataObject& globalDataFactory = ApiProvider::Get().GetGlobalDataFactory();
    return globalDataFactory.CreateInstance(spGlobalData);
}

std::experimental::generator<Object> DataObject::GetIterator(_In_ const Object& /*dataObject*/,
                                                             _In_ ComPtr<SymbolSet>& spSymbolSet)
{
    //
    // We must take **GREAT CARE** with what we touch and how we iterate.  After a co_yield, the state
    // of things may have drastically changed.  We must refetch things and only rely upon positional
    // counters!
    //
    size_t cur = 0;
    for(;;)
    {
        auto&& globalSymbols = spSymbolSet->InternalGetGlobalSymbols();
        if (cur >= globalSymbols.size())
        {
            break;
        }

        ULONG64 nextGlobal = globalSymbols[cur];
        ++cur;

        BaseSymbol *pNextSymbol = spSymbolSet->InternalGetSymbol(nextGlobal);
        if (pNextSymbol->InternalGetKind() != SvcSymbolData)
        {
            continue;
        }

        GlobalDataSymbol *pNextGlobalData = static_cast<GlobalDataSymbol *>(pNextSymbol);
        ComPtr<GlobalDataSymbol> spGlobalData = pNextGlobalData;
        GlobalDataObject& globalDataFactory = ApiProvider::Get().GetGlobalDataFactory();
        Object globalDataObject = globalDataFactory.CreateInstance(spGlobalData);
        co_yield globalDataObject;
    }
}

//*************************************************
// Global Data APIs:
//

Object GlobalDataObject::GetType(_In_ const Object& /*globalDataObject*/, 
                                 _In_ ComPtr<GlobalDataSymbol>& spGlobalDataSymbol)
{
    return BoxRelatedType(spGlobalDataSymbol.Get(), spGlobalDataSymbol->InternalGetSymbolTypeId());
}

void GlobalDataObject::SetType(_In_ const Object& /*globalDataObject*/, 
                               _In_ ComPtr<GlobalDataSymbol>& spGlobalDataSymbol,
                               _In_ Object globalDataType)
{
    BaseTypeSymbol *pNewDataType = UnboxType(spGlobalDataSymbol->InternalGetSymbolSet(),
                                             globalDataType);

    CheckHr(spGlobalDataSymbol->InternalSetSymbolTypeId(pNewDataType->InternalGetId()));
}

std::optional<ULONG64> GlobalDataObject::GetOffset(_In_ const Object& /*globalDataObject*/, 
                                                   _In_ ComPtr<GlobalDataSymbol>& spGlobalDataSymbol)
{
    std::optional<ULONG64> result;
    
    if (!spGlobalDataSymbol->InternalIsConstantValue())
    {
        result = spGlobalDataSymbol->InternalGetActualSymbolOffset();
    }
    return result;
}

void GlobalDataObject::SetOffset(_In_ const Object& /*globalDataObject*/,
                                 _In_ ComPtr<GlobalDataSymbol>& spGlobalDataSymbol,
                                 _In_ ULONG64 globalDataOffset)
{
    if (spGlobalDataSymbol->InternalIsConstantValue())
    {
        throw std::runtime_error("cannot set offset of global data which is constnat value");
    }

    CheckHr(spGlobalDataSymbol->InternalSetSymbolOffset(globalDataOffset));
}

std::wstring GlobalDataObject::ToString(_In_ const Object& /*globalDataObject*/,
                                        _In_ ComPtr<GlobalDataSymbol>& spGlobalDataSymbol,
                                        _In_ const Metadata& /*metadata*/)
{
    wchar_t buf[512];

    std::wstring const& dataName = spGlobalDataSymbol->InternalGetQualifiedName();

    ULONG64 dataTypeId = spGlobalDataSymbol->InternalGetSymbolTypeId();

    if (spGlobalDataSymbol->InternalIsConstantValue())
    {
        BaseSymbol *pDataTypeSymbol = spGlobalDataSymbol->InternalGetSymbolSet()->InternalGetSymbol(dataTypeId);
        std::wstring const& dataTypeName = pDataTypeSymbol->InternalGetQualifiedName();
        std::wstring value = ValueToString(spGlobalDataSymbol->InternalGetSymbolValue());

        swprintf_s(buf, ARRAYSIZE(buf), L"Global Data: %s (type = '%s', value = %s )",
                   dataName.empty() ? L"<Unknown>" : dataName.c_str(),
                   dataTypeName.empty() ? L"<Unknown>" : dataTypeName.c_str(),
                   value.c_str());
    }
    else
    {
        BaseSymbol *pDataTypeSymbol = spGlobalDataSymbol->InternalGetSymbolSet()->InternalGetSymbol(dataTypeId);
        std::wstring const& dataTypeName = pDataTypeSymbol->InternalGetQualifiedName();

        swprintf_s(buf, ARRAYSIZE(buf), L"Global Data: %s ( type = '%s', module offset = %d )",
                   dataName.empty() ? L"<Unknown>" : dataName.c_str(),
                   dataTypeName.empty() ? L"<Unknown>" : dataTypeName.c_str(),
                   (ULONG)spGlobalDataSymbol->InternalGetActualSymbolOffset());
    }

    return (std::wstring)buf;
}

void GlobalDataObject::Delete(_In_ const Object& /*globalDataObject*/,
                              _In_ ComPtr<GlobalDataSymbol>& spGlobalDataSymbol)
{
    CheckHr(spGlobalDataSymbol->Delete());
}

//*************************************************
// Data Model Bindings:
//
// The constructors for our extension points & typed model / factory objects set up all of the available properties,
// methods, etc...  which are available in the data model and which C++ methods act as the callbacks for fetching /
// setting properties, calling methods, etc...
//
// Each property or method may associate an optional metadata store which can have some key properties:
//
//     "Help"       - A string which provides "help text" for the property or method (e.g.: single line tool tip style)
//     "PreferShow" - Indicates whether the property/method should be shown by default (methods are hidden by default)
//

ModuleExtension::ModuleExtension() :
    ExtensionModel(NamedModelParent(L"Debugger.Models.Module"))
{
    AddReadOnlyProperty(L"SymbolBuilderSymbols", this, &ModuleExtension::GetSymbolBuilderSymbols,
                        Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_MODULE_SYMBOLBUILDERSYMBOLS }));
}
       
SymbolSetObject::SymbolSetObject() :
    TypedInstanceModel()
{
    AddReadOnlyProperty(L"Data", this, &SymbolSetObject::GetData,
                        Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_SYMBOLSET_DATA }));

    AddReadOnlyProperty(L"Types", this, &SymbolSetObject::GetTypes,
                        Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_SYMBOLSET_TYPES }));
}

TypesObject::TypesObject() :
    TypedInstanceModel()
{
    AddMethod(L"AddBasicCTypes", this, &TypesObject::AddBasicCTypes,
               Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_TYPES_ADDBASICCTYPES },
                        L"PreferShow", true));

    AddMethod(L"Create", this, &TypesObject::Create,
              Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_TYPES_CREATE },
                       L"PreferShow", true));

    AddMethod(L"CreateArray", this, &TypesObject::CreateArray,
              Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_TYPES_CREATEARRAY },
                       L"PreferShow", true));

    AddMethod(L"CreateEnum", this, &TypesObject::CreateEnum,
              Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_TYPES_CREATEENUM },
                       L"PreferShow", true));

    AddMethod(L"CreatePointer", this, &TypesObject::CreatePointer,
              Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_TYPES_CREATEPOINTER },
                       L"PreferShow", true));

    AddMethod(L"CreateTypedef", this, &TypesObject::CreateTypedef,
              Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_TYPES_CREATETYPEDEF },
                       L"PreferShow", true));

    AddGeneratorFunction(this, &TypesObject::GetIterator);
}

template<typename TSym>
BaseSymbolObject<TSym>::BaseSymbolObject() :
    TypedInstanceModel<ComPtr<TSym>>()
{
    this->AddReadOnlyProperty(L"Name", this, &BaseSymbolObject::GetName,
                              Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_SYMBOL_NAME }));

    this->AddReadOnlyProperty(L"QualifiedName", this, &BaseSymbolObject::GetQualifiedName,
                              Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_SYMBOL_QUALIFIEDNAME }));

    this->AddReadOnlyProperty(L"Parent", this, &BaseSymbolObject::GetParent,
                              Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_SYMBOL_PARENT }));
}

template<typename TType>
BaseTypeObject<TType>::BaseTypeObject(_In_ PCWSTR pwszConvTag) :
    m_pwszConvTag(pwszConvTag)
{
    this->AddReadOnlyProperty(L"Size", this, &BaseTypeObject::GetSize,
                              Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_TYPE_SIZE }));

    this->AddReadOnlyProperty(L"Alignment", this, &BaseTypeObject::GetAlignment,
                              Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_TYPE_ALIGNMENT }));

    this->AddStringDisplayableFunction(this, &BaseTypeObject::ToString);

    this->AddMethod(L"Delete", this, &BaseTypeObject::Delete,
                    Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_TYPE_DELETE }));
}

BasicTypeObject::BasicTypeObject() :
    BaseTypeObject(L"Basic Type")
{
}

UdtTypeObject::UdtTypeObject() :
    BaseTypeObject(L"UDT")
{
    AddReadOnlyProperty(L"BaseClasses", this, &UdtTypeObject::GetBaseClasses,
                        Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_UDTTYPE_BASECLASSES }));

    AddReadOnlyProperty(L"Fields", this, &UdtTypeObject::GetFields,
                        Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_UDTTYPE_FIELDS }));
}

PointerTypeObject::PointerTypeObject() :
    BaseTypeObject(L"Pointer")
{
    AddReadOnlyProperty(L"BaseType", this, &PointerTypeObject::GetBaseType,
                        Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_POINTERTYPE_BASETYPE }));
}

ArrayTypeObject::ArrayTypeObject() :
    BaseTypeObject(L"Array")
{
    AddReadOnlyProperty(L"ArraySize", this, &ArrayTypeObject::GetArraySize,
                        Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_ARRAYTYPE_ARRAYSIZE }));

    AddReadOnlyProperty(L"BaseType", this, &ArrayTypeObject::GetBaseType,
                        Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_ARRAYTYPE_BASETYPE }));
}

TypedefTypeObject::TypedefTypeObject() :
    BaseTypeObject(L"Typedef")
{
    AddReadOnlyProperty(L"BaseType", this, &TypedefTypeObject::GetBaseType,
                        Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_TYPEDEFTYPE_BASETYPE }));
}

EnumTypeObject::EnumTypeObject() :
    BaseTypeObject(L"Enum")
{
    AddReadOnlyProperty(L"BaseType", this, &EnumTypeObject::GetBaseType,
                        Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_ENUMTYPE_BASETYPE }));

    AddReadOnlyProperty(L"Enumerants", this, &EnumTypeObject::GetEnumerants,
                        Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_ENUMTYPE_ENUMERANTS }));
}

FieldsObject::FieldsObject() :
    TypedInstanceModel()
{
    AddMethod(L"Add", this, &FieldsObject::Add,
              Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_FIELDS_ADD },
                       L"PreferShow", true));

    AddGeneratorFunction(this, &FieldsObject::GetIterator);
}

EnumerantsObject::EnumerantsObject() :
    TypedInstanceModel()
{
    AddMethod(L"Add", this, &EnumerantsObject::Add,
              Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_ENUMERANTS_ADD },
                       L"PreferShow", true));

    AddGeneratorFunction(this, &EnumerantsObject::GetIterator);
}

FieldObject::FieldObject()
{
    AddStringDisplayableFunction(this, &FieldObject::ToString);

    AddProperty(L"IsAutomaticLayout", this, &FieldObject::GetIsAutomaticLayout, &FieldObject::SetIsAutomaticLayout,
                Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_FIELD_ISAUTOMATICLAYOUT }));

    AddProperty(L"Type", this, &FieldObject::GetType, &FieldObject::SetType,
                Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_FIELD_TYPE }));

    AddProperty(L"Offset", this, &FieldObject::GetOffset, &FieldObject::SetOffset,
                Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_FIELD_OFFSET }));

    AddMethod(L"Delete", this, &FieldObject::Delete,
              Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_FIELD_DELETE }));

    AddMethod(L"MoveBefore", this, &FieldObject::MoveBefore,
              Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_FIELD_MOVEBEFORE }));
}

BaseClassesObject::BaseClassesObject() :
    TypedInstanceModel()
{
    AddMethod(L"Add", this, &BaseClassesObject::Add,
              Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_BASECLASSES_ADD },
                       L"PreferShow", true));

    AddGeneratorFunction(this, &BaseClassesObject::GetIterator);
}

BaseClassObject::BaseClassObject()
{
    AddStringDisplayableFunction(this, &BaseClassObject::ToString);

    AddProperty(L"IsAutomaticLayout", this, &BaseClassObject::GetIsAutomaticLayout, &BaseClassObject::SetIsAutomaticLayout,
                Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_BASECLASS_ISAUTOMATICLAYOUT }));

    AddProperty(L"Type", this, &BaseClassObject::GetType, &BaseClassObject::SetType,
                Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_BASECLASS_TYPE }));

    AddProperty(L"Offset", this, &BaseClassObject::GetOffset, &BaseClassObject::SetOffset,
                Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_BASECLASS_OFFSET }));

    AddMethod(L"Delete", this, &BaseClassObject::Delete,
              Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_BASECLASS_DELETE }));

    AddMethod(L"MoveBefore", this, &BaseClassObject::MoveBefore,
              Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_BASECLASS_MOVEBEFORE }));
}

DataObject::DataObject()
{
    AddMethod(L"CreateGlobal", this, &DataObject::CreateGlobal,
              Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_DATA_CREATEGLOBAL },
                       L"PreferShow", true));

    AddGeneratorFunction(this, &DataObject::GetIterator);
}

GlobalDataObject::GlobalDataObject()
{
    AddStringDisplayableFunction(this, &GlobalDataObject::ToString);

    AddProperty(L"Type", this, &GlobalDataObject::GetType, &GlobalDataObject::SetType,
                Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_GLOBALDATA_TYPE }));

    AddProperty(L"Offset", this, &GlobalDataObject::GetOffset, &GlobalDataObject::SetOffset,
                Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_GLOBALDATA_OFFSET }));

    AddMethod(L"Delete", this, &GlobalDataObject::Delete,
              Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_GLOBALDATA_DELETE }));
}

SymbolBuilderNamespace::SymbolBuilderNamespace() :
    ExtensionModel(NamespacePropertyParent(L"Debugger.Models.Utility",
                                           L"Debugger.Models.Utility.SymbolBuilder",
                                           L"SymbolBuilder"))
{
    AddMethod(L"CreateSymbols", this, &SymbolBuilderNamespace::CreateSymbols,
              Metadata(L"Help", DeferredResourceString { SYMBOLBUILDER_IDS_CREATESYMBOLS },
                       L"PreferShow", true));
}

} // SymbolBuilder
} // Libraries
} // DataModel
} // Debugger

//*************************************************
// Core Initialization:
//

using namespace Debugger::DataModel::Libraries::SymbolBuilder;

void UninitializeObjectModel()
{
    if (g_pProvider != nullptr)
    {
        delete g_pProvider;
        g_pProvider = nullptr;
    }

    if (g_pManager != nullptr)
    {
        g_pManager->Release();
        g_pManager = nullptr;
    }

    if (g_pHost != nullptr)
    {
        g_pHost->Release();
        g_pHost = nullptr;
    }
}

HRESULT InitializeObjectModel(_In_ IUnknown *pHostInterface)
{
    HRESULT hr = S_OK;

    //
    // DbgModelClientEx.h providers throw exceptions.  Most of that is managed by the extension itself
    // and it does all the requisite translation.  The original initialization where we hook things up is an
    // exception to this.  We must bound any C++ exception which comes out.
    //
    try
    {
        ComPtr<IHostDataModelAccess> spAccess;

        hr = pHostInterface->QueryInterface(IID_PPV_ARGS(&spAccess));
        if (SUCCEEDED(hr))
        {
            hr = spAccess->GetDataModel(&g_pManager, &g_pHost);
        }

        if (SUCCEEDED(hr))
        {
            g_pProvider = new ApiProvider;
        }
    }
    catch(...)
    {
        hr = E_FAIL;
    }

    if (FAILED(hr))
    {
        UninitializeObjectModel();
    }

    return hr;
}

