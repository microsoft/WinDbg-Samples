//**************************************************************************
//
// ScriptProvider.cpp
//
// The core script provider for Python.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#include "PyProvider.h"

using namespace Microsoft::WRL;
using namespace Debugger::DataModel;
using namespace Debugger::DataModel::ClientEx;
using namespace Debugger::DataModel::ScriptProvider;
using namespace Debugger::DataModel::ScriptProvider::Python;

namespace Debugger::DataModel::ScriptProvider::Python
{

PythonProvider *PythonProvider::s_pScriptProvider = nullptr;
PythonProvider::ProviderState PythonProvider::s_State = PythonProvider::ProviderState::Uninitialized;

//*************************************************
// Public APIs:
//

HRESULT PythonProvider::GetName(_Out_ BSTR *pbstrName)
{
    *pbstrName = SysAllocString(L"Python");
    if (*pbstrName == nullptr)
    {
        return E_OUTOFMEMORY;
    }

    return S_OK;
}

HRESULT PythonProvider::GetExtension(_Out_ BSTR *pbstrExtension)
{
    *pbstrExtension = SysAllocString(L"py");
    if (*pbstrExtension == nullptr)
    {
        return E_OUTOFMEMORY;
    }

    return S_OK;
}

HRESULT PythonProvider::CreateScript(_COM_Outptr_ IDataModelScript **ppScript)
{
    HRESULT hr = S_OK;
    *ppScript = nullptr;

    ComPtr<PythonScript> spScript;
    IfFailedReturn(MakeAndInitialize<PythonScript>(&spScript, this));

    *ppScript = spScript.Detach();
    return hr;
}

HRESULT PythonProvider::GetDefaultTemplateContent(_COM_Outptr_ IDataModelScriptTemplate **ppTemplateContent)
{
    HRESULT hr = S_OK;
    *ppTemplateContent = nullptr;

    ComPtr<PythonScriptTemplate> spTemplate;
    IfFailedReturn(MakeAndInitialize<PythonScriptTemplate>(&spTemplate, GetDefaultTemplateData()));

    *ppTemplateContent = spTemplate.Detach();
    return hr;
}

HRESULT PythonProvider::EnumerateTemplates(_COM_Outptr_ IDataModelScriptTemplateEnumerator **ppTemplateEnumerator)
{
    HRESULT hr = S_OK;
    *ppTemplateEnumerator = nullptr;

    ComPtr<PythonScriptTemplateEnumerator> spTemplateEnumerator;
    IfFailedReturn(MakeAndInitialize<PythonScriptTemplateEnumerator>(&spTemplateEnumerator));

    *ppTemplateEnumerator = spTemplateEnumerator.Detach();
    return hr;
}

HRESULT PythonProvider::RuntimeClassInitialize(_In_ IDataModelManager *pManager,
                                               _In_ IDataModelScriptManager *pScriptManager,
                                               _In_ IDebugHostScriptHost *pScriptHost)
{
    HRESULT hr = S_OK;

    m_spManager = pManager;
    m_spScriptManager = pScriptManager;
    m_spScriptHost = pScriptHost;

    IfFailedReturn(m_spScriptHost.As(&m_spHost));
    IfFailedReturn(m_spScriptHost.As(&m_spHostSymbols));
    IfFailedReturn(m_spScriptHost.As(&m_spHostEvaluator));
    IfFailedReturn(m_spScriptHost.As(&m_spHostMemory));
    IfFailedReturn(m_spScriptHost.As(&m_spHostStatus));
    (void)m_spScriptHost.As(&m_spHostExtensibility); // optional!

    ComPtr<IDataModelNameBinder> spNameBinder;
    IfFailedReturn(pScriptManager->GetDefaultNameBinder(&spNameBinder));

    //
    // For any resource strings...
    //
    if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCTSTR>(&PythonProvider::Get),
                          &m_resourceModule) == FALSE)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    PyConfig config;
    PyConfig_InitIsolatedConfig(&config);
    config.isolated = 1;

    IfStatusErrorConvertAndReturn(Py_InitializeFromConfig(&config));
    if (Marshal::DataModelSourceObject::StaticInitialize() < 0)
    {
        return E_FAIL;
    }

    //
    // @TODO: The Python documentation around this is *ABSOLUTELY ABYSMAL*.  It talks about merely calling
    //        PyGILState_Ensure / PyGILState_Release from alternate threads in order to safely call back into
    //        Python code.  Unfortunately, any of the initializers seem to take and never release the GIL leaving
    //        any background thread calling PyGILState_Ensure in a deadlock.
    //
    //        I can find a plethora of references to this on StackOverflow and elsewhere and most of them
    //        refer to calling methods which are deprecated long before Python 3.11.  I have absolutely no idea
    //        if this is the correct thing to do...
    //
    //        But the various threads calling into Python are guarded and will always PyGILState_Ensure, so
    //        we need to make sure that DOES NOT DEADLOCK LEFT AND RIGHT.
    //
    //        Sigh...  Half-baked documentation is irritating.
    //
    PyThreadState *pNukedState = PyEval_SaveThread();

    m_spMarshaler.reset(new(std::nothrow) Marshal::PythonMarshaler(this, pManager, spNameBinder.Get()));
    if (m_spMarshaler == nullptr)
    {
        return E_OUTOFMEMORY;
    }
    IfFailedReturn(m_spMarshaler->Initialize());

    return hr;
}

HRESULT PythonProvider::GetStringResource(_In_ ULONG rscId,
                                          _Out_ std::unique_ptr<char[]> *pStringResource)
{
    HRESULT hr = S_OK;
    pStringResource->reset(nullptr);

    PSTR pszStr;
    INT result = ::LoadStringA(m_resourceModule, rscId, reinterpret_cast<PSTR>(&pszStr), 0);
    if (!result)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    std::unique_ptr<char[]> spStr(new(std::nothrow) char[result + 1]);
    if (spStr == nullptr)
    {
        return E_OUTOFMEMORY;
    }

    strncpy_s(spStr.get(), result + 1, pszStr, result);
    *pStringResource = std::move(spStr);
    return hr;
}

void PythonProvider::FinishInitialization()
{
    //*************************************************
    // DANGER:
    //
    // After this point, there is a static non-RAII reference to the canonical provider.  It
    // *MUST* be uninitialized via part of the unload cycle or explicit clean-up code.
    //
    s_State = ProviderState::Registered;
    s_pScriptProvider = this;
    AddRef();
}

HRESULT PythonHostRegistration::Initialize(_In_ PythonScriptState *pScriptState, _In_ PyObject *pPyBridgeElement)
{
    HRESULT hr = S_OK;

    PythonProvider *pProvider = pScriptState->GetScript()->GetProvider();
    Marshal::PythonMarshaler *pMarshaler = pProvider->GetMarshaler();
    Library::PythonLibrary *pPythonLibrary = pScriptState->GetPythonLibrary();
    Library::HostLibrary *pHostLibrary = pPythonLibrary->GetHostLibrary();
    IDataModelManager *pDataModelManager = pProvider->GetDataModelManager();
    m_pScriptState = pScriptState;

    Library::HostRegistrationKind regKind;
    IfFailedReturn(pHostLibrary->GetRegistrationKind(pPyBridgeElement, &regKind));

    Object marshaledObject;
    Metadata marshaledMetadata;

    std::wstring nameOrSignature;
    std::wstring moduleName;
    std::wstring minVersion;
    std::wstring maxVersion;
    PinnedReference registeredClass;
    PyObject *pPyObject;
    ComPtr<IDebugHostTypeSignature> spSig;

    switch(regKind)
    {
#ifdef OTHERREC
        case Library::HostRegistrationKind::OptionalRecord:
        {
            IfFailedReturn(pHostLibrary->GetUnderlyingRecord(pPyBridgeElement, &pPyObject));
            auto underlyingRecord = PinnedReference::Take(pPyObject);
            IfFailedReturn(Initialize(pDataModelManager, regKind, pScriptState, underlyingRecord));
            break;
        }

        case Library::HostRegistrationKind::FunctionAlias:
        {
#if 0
            JsValueRef jsMetadata;
#endif // 0

            IfFailedReturn(pHostLibrary->GetFunctionAliasInformation(pPyBridgeElement,
                                                                     &pwszNameOrSignature,
                                                                     &pPyObject));
#if 0
                                                                     &jsMetadata));
#endif // 0
            auto function = PinnedReference::Take(pPyObject);

            Client::Metadata metadata;
#if 0
            if (jsMetadata != nullptr)
            {
                IfFailedReturn(pMarshaler->CreateMetadataFromDescriptor(jsMetadata, nullptr, &metadata));
            }
#endif // 0

            IfFailedReturn(pMarshaler->MarshalFromPython(function, &marshaledObject, nullptr, false));

            IfFailedReturn(Initialize(pDataModelManager, regKind, pwszNameOrSignature, marshaledObject, metadata));
            break;
        }
#endif // OTHERREC

        case Library::HostRegistrationKind::TypeSignatureRegistration:
        case Library::HostRegistrationKind::TypeSignatureExtension:
        {
            PCWSTR pwszReportName = (regKind == Library::HostRegistrationKind::TypeSignatureRegistration) ?
                L"TypeSignatureRegistration" : L"TypeSignatureExtension";

            IfFailedReturn(pHostLibrary->GetSignatureInformation(pPyBridgeElement, 
                                                                 &nameOrSignature,
                                                                 &moduleName,
                                                                 &minVersion,
                                                                 &maxVersion,
                                                                 &registeredClass));

            if (!moduleName.empty())
            {
                hr = pProvider->GetHostSymbols()->CreateTypeSignatureForModuleRange(nameOrSignature.c_str(),
                                                                                    moduleName.c_str(),
                                                                                    minVersion.empty() ? nullptr : minVersion.c_str(),
                                                                                    maxVersion.empty() ? nullptr : maxVersion.c_str(),
                                                                                    &spSig);
            }
            else
            {
                if (!minVersion.empty() || !maxVersion.empty())
                {
                    pScriptState->GetScript()->ReportError(ErrorClassError, E_INVALIDARG, IDS_INVALID_ARGUMENT, pwszReportName);
                    return E_INVALIDARG;
                }

                hr = pProvider->GetHostSymbols()->CreateTypeSignature(nameOrSignature.c_str(), nullptr, &spSig);
            }

            if (FAILED(hr))
            {
#if 0
                pScriptState->GetScript()->ReportError(ErrorClassError, 
                                                       hr, 
                                                       IDS_CANNOT_MODIFY_OBJECT_MODEL,
                                                       pwszReportName,
                                                       pwszNameOrSignature);
#endif // 0
                return hr;
            }

            IfFailedReturn(pMarshaler->MarshalFromPython(registeredClass, &marshaledObject, &marshaledMetadata, true));

            //
            // Verify that it marshaled out with a valid data model.  If not, fail immediately.
            //
            ComPtr<IDataModelConcept> spDataModel;
            IfFailedReturn(marshaledObject->GetConcept(__uuidof(IDataModelConcept), &spDataModel, nullptr));

            IfFailedReturn(Initialize(pDataModelManager, regKind, nameOrSignature.c_str(), spSig.Get(), marshaledObject));
            break;
        }

#ifdef OTHERREC

        case Library::HostRegistrationKind::NamedModelRegistration:
        case Library::HostRegistrationKind::NamedModelParent:
        {
            IfFailedReturn(pHostLibrary->GetModelInformation(pPyBridgeElement,
                                                             &pwszNameOrSignature,
                                                             &pPyRegisteredClass));
            auto registeredClass = PinnedReference::Take(pPyRegisteredClass);

            //
            // This implies that we track the model name for the first registration.  This is never undone --
            // it is simply tracked.  Perform the action here.
            //
            if (regKind == Library::HostRegistrationKind::NamedModelRegistration)
            {
                return E_NOTIMPL;
#if 0
                JsRef jsModelName;
                IfErrorConvertAndReturn(JsGetIndexedProperty(jsRegisteredPrototype, 
                                                             pJsLibrary->GetModelNameSymbol(),
                                                             &jsModelName));

                JsValueType jsType;
                IfErrorConvertAndReturn(JsGetValueType(jsModelName, &jsType));
                if (jsType == JsUndefined)
                {
                    IfErrorConvertAndReturn(JsPointerToString(pwszNameOrSignature, 
                                                              wcslen(pwszNameOrSignature), 
                                                              &jsModelName));

                    IfErrorConvertAndReturn(JsSetIndexedProperty(jsRegisteredPrototype, 
                                                                 pJsLibrary->GetModelNameSymbol(),
                                                                 jsModelName));
                }
#endif // 0
            }

            IfFailedReturn(pMarshaler->MarshalFromPython(registeredClass, &marshaledObject, &marshaledMetadata, true));

            //
            // Verify that it marshaled out with a valid data model.  If not, fail immediately.
            //
            ComPtr<IDataModelConcept> spDataModel;
            IfFailedReturn(marshaledObject->GetConcept(__uuidof(IDataModelConcept), &spDataModel, nullptr));

            IfFailedReturn(Initialize(pDataModelManager, regKind, pwszNameOrSignature, marshaledObject));
            break;
        }

        case Library::HostRegistrationKind::NamespacePropertyParent:
        {
            PCWSTR ppwszSubNamespaceModelName;
            PCWSTR ppwszSubNamespaceAccessName;

            IfFailedReturn(pHostLibrary->GetSubNamespaceInformation(pPyBridgeElement,
                                                                    &pwszNameOrSignature,
                                                                    &ppwszSubNamespaceModelName,
                                                                    &ppwszSubNamespaceAccessName,
                                                                    &pPyRegisteredClass));
            auto registeredClass = PinnedReference::Take(pPyRegisteredClass);

            IfFailedReturn(pMarshaler->MarshalFromPython(registeredClass, &marshaledObject, &marshaledMetadata, true));

            //
            // Verify that it marshaled out with a valid data model.  If not, fail immediately.
            //
            ComPtr<IDataModelConcept> spDataModel;
            IfFailedReturn(marshaledObject->GetConcept(__uuidof(IDataModelConcept), &spDataModel, nullptr));

            IfFailedReturn(Initialize(pDataModelManager, 
                                      regKind, 
                                      pwszNameOrSignature, 
                                      ppwszSubNamespaceModelName, 
                                      ppwszSubNamespaceAccessName,
                                      marshaledObject));
            break;
        }

        case Library::HostRegistrationKind::ApiVersionSupport:
        {
            ULONG majorVersion;
            ULONG minorVersion;

            IfFailedReturn(pHostLibrary->GetApiVersionSupportInformation(pPyBridgeElement, &majorVersion, &minorVersion));

            IfFailedReturn(Initialize(pDataModelManager, regKind, majorVersion, minorVersion));
            break;
        }

        case Library::HostRegistrationKind::ResourceFile:
        {
            PCWSTR ppwszResourceFileName;

            IfFailedReturn(pHostLibrary->GetResourceFileName(pPyBridgeElement, &ppwszResourceFileName));

            IfFailedReturn(Initialize(pDataModelManager, regKind, ppwszResourceFileName));
            break;
        }

        case Library::HostRegistrationKind::AllowOutsidePropertyWrites:
        {
            bool allowOutsidePropertyWrites;

            IfFailedReturn(pHostLibrary->GetAllowOutsidePropertyWritesInformation(pPyBridgeElement, &allowOutsidePropertyWrites));

            IfFailedReturn(Initialize(pDataModelManager, regKind, allowOutsidePropertyWrites));
            break;
        }
#endif // OTHERREC

        default:
        {
            return E_UNEXPECTED;
        }
    }

    return hr;
}

HRESULT PythonHostRegistration::Initialize(_In_ IDataModelManager *pDataModelManager,
                                           _In_ Library::HostRegistrationKind regKind,
                                           _In_ PythonScriptState *pScriptState,
                                           _In_ PyObject *pPyUnderlyingRecord)
{
    HRESULT hr = S_OK;
    m_spDataModelManager = pDataModelManager;
    m_registrationKind = regKind;
    m_spUnderlyingRecord.reset(new(std::nothrow) PythonHostRegistration);
    if (m_spUnderlyingRecord.get() == nullptr)
    {
        return E_OUTOFMEMORY;
    }

    IfFailedReturn(m_spUnderlyingRecord->Initialize(pScriptState, pPyUnderlyingRecord));
    return hr;
}

HRESULT PythonHostRegistration::Initialize(_In_ IDataModelManager *pDataModelManager,
                                               _In_ Library::HostRegistrationKind regKind,
                                               _In_ PCWSTR pwszSignature,
                                               _In_ IDebugHostTypeSignature *pTypeSignature,
                                               _In_ const Object& marshaledDataModel)
{
    HRESULT hr = S_OK;
    auto fn = [&](){
        m_signature = pwszSignature;
        m_spDataModelManager = pDataModelManager;
        m_registrationKind = regKind;
        m_spTypeSignature = pTypeSignature;
        m_marshaledObject = marshaledDataModel;
        return hr;
    };
    IfFailedReturn(ConvertException(fn));
    return hr;
}

HRESULT PythonHostRegistration::Initialize(_In_ IDataModelManager *pDataModelManager,
                                           _In_ Library::HostRegistrationKind regKind,
                                           _In_ PCWSTR pwszName,
                                           _In_ const Object& marshaledObject,
                                           _In_ const Metadata & marshaledMetadata /* = Metadata() */)
{
    HRESULT hr = S_OK;

    auto fn = [&](){
        m_spDataModelManager = pDataModelManager;
        m_registrationKind = regKind;
        m_name = pwszName;
        m_marshaledObject = marshaledObject;
        m_marshaledMetadata = marshaledMetadata;
        return hr;
    };
    IfFailedReturn(ConvertException(fn));
    return hr;
}

HRESULT PythonHostRegistration::Initialize(_In_ IDataModelManager *pDataModelManager,
                                           _In_ Library::HostRegistrationKind regKind,
                                           _In_ PCWSTR pwszModelName,
                                           _In_ PCWSTR pwszSubNamespaceModelName,
                                           _In_ PCWSTR pwszSubNamespaceAccessName,
                                           _In_ const Object& marshaledObject)
{
    HRESULT hr = S_OK;

    auto fn = [&](){

        if (regKind != Library::HostRegistrationKind::NamespacePropertyParent)
        {
            return E_FAIL;
        }

        m_spDataModelManager = pDataModelManager;
        m_registrationKind = regKind;
        m_name = pwszModelName;
        m_subNamespaceModelName = pwszSubNamespaceModelName;
        m_subNamespaceAccessName = pwszSubNamespaceAccessName;
        m_marshaledObject = marshaledObject;
        return hr;
    };
    IfFailedReturn(ConvertException(fn));
    return hr;
}

HRESULT PythonHostRegistration::Initialize(_In_ IDataModelManager *pDataModelManager,
                                           _In_ Library::HostRegistrationKind regKind,
                                           _In_ ULONG majorVersion,
                                           _In_ ULONG minorVersion)
{
    HRESULT hr = S_OK;
    m_spDataModelManager = pDataModelManager;
    m_registrationKind = regKind;
    m_majorVersion = majorVersion;
    m_minorVersion = minorVersion;
    return hr;
}

HRESULT PythonHostRegistration::Initialize(_In_ IDataModelManager *pDataModelManager,
                                           _In_ Library::HostRegistrationKind regKind,
                                           _In_ PCWSTR pwszResourceFileName)
{
    auto fn = [&]() {
        m_spDataModelManager = pDataModelManager;
        m_registrationKind = regKind;
        m_resourceFileName = pwszResourceFileName;

        return S_OK;
    };

    return ConvertException(fn);
}

HRESULT PythonHostRegistration::Initialize(_In_ IDataModelManager *pDataModelManager,
                                           _In_ Library::HostRegistrationKind regKind,
                                           _In_ bool allowOutsidePropertyWrites)
{
    auto fn = [&]() {
        m_spDataModelManager = pDataModelManager;
        m_registrationKind = regKind;
        m_allowOutsidePropertyWrites = allowOutsidePropertyWrites;

        return S_OK;
    };

    return ConvertException(fn);
}

HRESULT PythonHostRegistration::Apply(_Out_ bool *pActiveRecord, _In_ bool isOptional)
{
    HRESULT hr = S_OK;
    *pActiveRecord = true;

    switch(m_registrationKind)
    {
        case Library::HostRegistrationKind::OptionalRecord:
        {
            //
            // If the underlying record type is optional, a failure will not cause the application to fail.  It will
            // just pass back an indication that the record is not active.
            //
            bool childActive;
            assert(m_spUnderlyingRecord.get() != nullptr);
            if (FAILED(m_spUnderlyingRecord->Apply(&childActive, true)))
            {
                *pActiveRecord = false;
            }
            else
            {
                *pActiveRecord = childActive;
            }
            
            break;
        }

        case Library::HostRegistrationKind::FunctionAlias:
        {
            PythonProvider *pProvider = m_pScriptState->GetScript()->GetProvider();
            ComPtr<IDebugHostExtensibility> spExtensibility = pProvider->GetHostExtensibility();
            if (spExtensibility == nullptr)
            {
                return E_NOTIMPL;
            }

            ComPtr<IDebugHostExtensibility2> spExtensibility2;
            if (SUCCEEDED(spExtensibility.As<IDebugHostExtensibility2>(&spExtensibility2)))
            {
                hr = spExtensibility2->CreateFunctionAliasWithMetadata(m_name.c_str(), m_marshaledObject, m_marshaledMetadata);
            }
            else
            {
                hr = spExtensibility->CreateFunctionAlias(m_name.c_str(), m_marshaledObject);
            }

            if (FAILED(hr))
            {
                if (!isOptional)
                {
                    m_pScriptState->GetScript()->ReportError(ErrorClassError, 
                                                             hr, 
                                                             IDS_CANNOT_MODIFY_OBJECT_MODEL, 
                                                             L"functionAlias",
                                                             m_name.c_str());
                }
                return hr;
            }
            break;
        }

        case Library::HostRegistrationKind::TypeSignatureRegistration:
            hr = m_spDataModelManager->RegisterModelForTypeSignature(m_spTypeSignature.Get(), m_marshaledObject);
            if (FAILED(hr))
            {
                if (!isOptional)
                {
                    m_pScriptState->GetScript()->ReportError(ErrorClassError,
                                                             hr,
                                                             IDS_CANNOT_MODIFY_OBJECT_MODEL,
                                                             L"typeSignatureRegistration",
                                                             m_signature.c_str());
                }
                                
                return hr;
            }
            break;

        case Library::HostRegistrationKind::TypeSignatureExtension:
            hr = m_spDataModelManager->RegisterExtensionForTypeSignature(m_spTypeSignature.Get(), m_marshaledObject);
            if (FAILED(hr))
            {
                if (!isOptional)
                {
                    m_pScriptState->GetScript()->ReportError(ErrorClassError,
                                                             hr,
                                                             IDS_CANNOT_MODIFY_OBJECT_MODEL,
                                                             L"typeSignatureExtension",
                                                             m_signature.c_str());
                }
                return hr;
            }
            break;

        case Library::HostRegistrationKind::NamedModelRegistration:
            hr = m_spDataModelManager->RegisterNamedModel(m_name.c_str(), m_marshaledObject);
            if (FAILED(hr))
            {
                if (!isOptional)
                {
                    m_pScriptState->GetScript()->ReportError(ErrorClassError,
                                                             hr,
                                                             IDS_CANNOT_MODIFY_OBJECT_MODEL,
                                                             L"namedModelRegistration",
                                                             m_name.c_str());
                }
                return hr;
            }
            break;

        case Library::HostRegistrationKind::NamedModelParent:
        {
            Microsoft::WRL::ComPtr<IModelObject> spOriginalNamedModel;
            hr = m_spDataModelManager->AcquireNamedModel(m_name.c_str(), &spOriginalNamedModel);
            m_originalNamedModel = std::move(spOriginalNamedModel);
            if (SUCCEEDED(hr))
            {
                hr = m_originalNamedModel->AddParentModel(m_marshaledObject, nullptr, false);
            }

            if (FAILED(hr))
            {
                if (!isOptional)
                {
                    m_pScriptState->GetScript()->ReportError(ErrorClassError,
                                                             hr,
                                                             IDS_CANNOT_MODIFY_OBJECT_MODEL,
                                                             L"namedModelParent",
                                                             m_name.c_str());
                }
                return hr;
            }

            break;
        }

        case Library::HostRegistrationKind::NamespacePropertyParent:
        {
            ComPtr<IDataModelManager2> spManager2;

            hr = m_spDataModelManager->QueryInterface(IID_PPV_ARGS(&spManager2));
            if (SUCCEEDED(hr))
            {
                ComPtr<IModelObject> spOriginalNamedModel;
                hr = spManager2->AcquireSubNamespace(m_name.c_str(),
                                                     m_subNamespaceModelName.c_str(),
                                                     m_subNamespaceAccessName.c_str(),
                                                     nullptr,
                                                     &spOriginalNamedModel);
                m_originalNamedModel = std::move(spOriginalNamedModel);

                if (SUCCEEDED(hr))
                {
                    hr = m_originalNamedModel->AddParentModel(m_marshaledObject, nullptr, false);
                }
            }

            if (FAILED(hr))
            {
                if (!isOptional)
                {
                    m_pScriptState->GetScript()->ReportError(ErrorClassError,
                                                             hr,
                                                             IDS_CANNOT_MODIFY_OBJECT_MODEL,
                                                             L"namespacePropertyParent",
                                                             m_name.c_str());
                }
            }

            break;
        }

#if 0
        case Library::HostRegistrationKind::ApiVersionSupport:
            IfFailedReturn(m_pScriptState->GetPythonLibrary()->InitializeApiVersionSupport(m_majorVersion, m_minorVersion));
            break;

        case Library::HostRegistrationKind::ResourceFile:
            IfFailedReturn(m_pScriptState->GetPythonLibrary()->InitializeResourceFileName(m_resourceFileName.c_str()));
            break;

        case Library::HostRegistrationKind::AllowOutsidePropertyWrites:
            IfFailedReturn(m_pScriptState->GetPythonLibrary()->InitializeAllowOutsidePropertyWrites(m_allowOutsidePropertyWrites));
            break;
#endif // 0

        default:
            return E_UNEXPECTED;
    }

    return hr;
}

HRESULT PythonHostRegistration::Undo()
{
    HRESULT hr = S_OK;

    switch(m_registrationKind)
    {
        case Library::HostRegistrationKind::OptionalRecord:
        {
            assert(m_spUnderlyingRecord.get() != nullptr);
            IfFailedReturn(m_spUnderlyingRecord->Undo());
            break;
        }

        case Library::HostRegistrationKind::FunctionAlias:
        {
            PythonProvider *pProvider = m_pScriptState->GetScript()->GetProvider();
            IDebugHostExtensibility *pExtensibility = pProvider->GetHostExtensibility();
            if (pExtensibility == nullptr)
            {
                return E_NOTIMPL;
            }

            IfFailedReturn(pExtensibility->DestroyFunctionAlias(m_name.c_str()));
            break;
        }

        case Library::HostRegistrationKind::TypeSignatureRegistration:
            IfFailedReturn(m_spDataModelManager->UnregisterModelForTypeSignature(m_marshaledObject, m_spTypeSignature.Get()));
            break;

        case Library::HostRegistrationKind::TypeSignatureExtension:
            IfFailedReturn(m_spDataModelManager->UnregisterExtensionForTypeSignature(m_marshaledObject, m_spTypeSignature.Get()));
            break;

        case Library::HostRegistrationKind::NamedModelRegistration:
            IfFailedReturn(m_spDataModelManager->UnregisterNamedModel(m_name.c_str()));
            break;

        case Library::HostRegistrationKind::NamedModelParent:
            IfFailedReturn(m_originalNamedModel->RemoveParentModel(m_marshaledObject));
            break;

        case Library::HostRegistrationKind::NamespacePropertyParent:
            IfFailedReturn(m_originalNamedModel->RemoveParentModel(m_marshaledObject));
            break;

        case Library::HostRegistrationKind::ApiVersionSupport:
        case Library::HostRegistrationKind::AllowOutsidePropertyWrites:
            //
            // This is meaningless...  There is nothing to "undo".  This had effect only on the JS side of the fence.
            //
            break;

    }

    return hr;
}

HRESULT PythonScript::RuntimeClassInitialize(_In_ PythonProvider *pScriptProvider)
{
    HRESULT hr = S_OK;

    IDebugHostScriptHost *pScriptHost = pScriptProvider->GetScriptHost();

    m_spProvider = pScriptProvider;
    IfFailedReturn(pScriptHost->CreateContext(this, &m_spScriptHostContext));

    ComPtr<IModelObject> spHostNamespace;
    IfFailedReturn(m_spScriptHostContext->GetNamespaceObject(&spHostNamespace));
    m_hostNamespace = std::move(spHostNamespace);

    return hr;
}

HRESULT PythonScript::InternalReportError(_In_ ErrorClass errClass,
                                          _In_ HRESULT hrError,
                                          _In_ ULONG line,
                                          _In_ ULONG pos,
                                          _In_ PCSTR pszMsg,
                                          _In_ va_list va)
{
    HRESULT hr = S_OK;

    std::wstring msgUtf16;
    IfFailedReturn(GetUTF16(pszMsg, &msgUtf16));

    size_t len = 1024;
    wchar_t msg[1024];
    wchar_t *pMsg = msg;
    std::unique_ptr<wchar_t[]> spXBuf;
    
    for(;;)
    {
        int cb = _vsnwprintf_s(pMsg, len, _TRUNCATE, msgUtf16.c_str(), va);
        if (cb == -1)
        {
            len *= 2;
            spXBuf.reset(new(std::nothrow) wchar_t[len]);
            if (spXBuf == nullptr)
            {
                va_end(va);
                return E_OUTOFMEMORY;
            }
            pMsg = spXBuf.get();
        }
        else
        {
            break;
        }
    }

    if (m_spReportingClient != nullptr)
    {
        hr = m_spReportingClient->ReportError(errClass, hrError, pMsg, line, pos);
    }

    return hr;
}

HRESULT PythonScript::InternalReportError(_In_ ErrorClass errClass,
                                          _In_ HRESULT hrError,
                                          _In_ ULONG line,
                                          _In_ ULONG pos,
                                          _In_ ULONG rscId,
                                          _In_ va_list va)
{
    HRESULT hr = S_OK;

    std::unique_ptr<char[]> spMsg;
    hr = m_spProvider->GetStringResource(rscId, &spMsg);
    if (FAILED(hr))
    {
        va_end(va);
        return hr;
    }

    IfFailedReturn(InternalReportError(errClass, hrError, line, pos, spMsg.get(), va));
    return hr;
}

HRESULT PythonScript::ReportExceptionOrError(_In_ HRESULT hrIn,
                                             _Out_ HRESULT *pConvertedResult,
                                             _In_ ErrorClass errClass,
                                             _In_ ULONG rscId,
                                             ...)
{
    HRESULT hr = S_OK;
    assert(FAILED(hrIn));

    *pConvertedResult = hrIn; // @TODO: Better conversion

    va_list va;
    va_start(va, rscId);

    std::unique_ptr<wchar_t[]> spMsg;

    PCSTR pszMsg = nullptr;
    ULONG line = 0;
    ULONG pos = 0;

    bool hasException = PyErr_Occurred();
    if (hasException)
    {
        PyObject *pType, *pValue, *pTraceback;
        PyErr_Fetch(&pType, &pValue, &pTraceback);
        PyErr_NormalizeException(&pType, &pValue, &pTraceback);

        auto type = PinnedReference::Take(pType);
        auto value = PinnedReference::Take(pValue);
        auto traceback = PinnedReference::Take(pTraceback);

#if 0
        //
        // This routine needs to be able to run before the library is fully initialized.  Hence, we **MUST**
        // deal with getting the properties of an exception here in the event that the library routines fail.
        //
        Library::PythonLibrary *pPythonLibrary = GetPythonLibrary();
        if (pPythonLibrary != nullptr)
        {
            IfFailedReturn(pPythonLibrary->GetExceptionDetails(type, value, traceback, &pwszMsg, &line, &pos));
        }
        else
#endif // 0
        {
            //
            // We need to fall back to calling each method by hand.
            //
            if (PyObject_IsInstance(value, PyExc_SyntaxError))
            {
                if (PyObject_HasAttrString(value, "lineno"))
                {
                    PyObject *pLineObj = PyObject_GetAttrString(value, "lineno");
                    IfObjectErrorConvertAndReturn(pLineObj);
                    auto lineObj = PinnedReference::Take(pLineObj);
                    line = static_cast<ULONG>(PyLong_AsLong(lineObj));
                }

                if (PyObject_HasAttrString(value, "offset"))
                {
                    PyObject *pOffsetObj = PyObject_GetAttrString(value, "offset");
                    IfObjectErrorConvertAndReturn(pOffsetObj);
                    auto offsetObj = PinnedReference::Take(pOffsetObj);
                    pos = static_cast<ULONG>(PyLong_AsLong(offsetObj));
                }

                if (PyObject_HasAttrString(value, "msg"))
                {
                    PyObject *pMsgObj = PyObject_GetAttrString(value, "msg");
                    IfObjectErrorConvertAndReturn(pMsgObj);
                    auto msgObj = PinnedReference::Take(pMsgObj);
                    pszMsg = PyUnicode_AsUTF8AndSize(msgObj, nullptr);
                }
            }
            else if (value != nullptr)
            {
                PyObject *pStrObj = PyObject_Str(value);
                auto strObj = PinnedReference::Take(pStrObj);
                pszMsg = PyUnicode_AsUTF8AndSize(pStrObj, nullptr);
            }
        }

        //
        // If for any reason, we could not find a specific message and information from an exception object,
        // fall back to utilizing the more generic error message.
        //
        if (pszMsg != nullptr)
        {
            IfFailedReturn(InternalReportError(errClass, *pConvertedResult, line, pos, pszMsg, va));
        }
        else
        {
            IfFailedReturn(InternalReportError(errClass, *pConvertedResult, 0, 0, rscId, va));
        }
    }
    else
    {
        IfFailedReturn(InternalReportError(errClass, *pConvertedResult, 0, 0, rscId, va));
    }

    return hr;
}


Marshal::PythonMarshaler* PythonScript::GetMarshaler() const
{
    return m_spProvider->GetMarshaler();
}

HRESULT PythonScript::GetName(_Out_ BSTR *pbstrName)
{
    if (m_scriptName.empty())
    {
        *pbstrName = nullptr;
    }
    else
    {
        *pbstrName = SysAllocString(m_scriptName.c_str());
        if (*pbstrName == nullptr)
        {
            return E_OUTOFMEMORY;
        }
    }

    return S_OK;
}

HRESULT PythonScript::Rename(_In_ PCWSTR pwszScriptName)
{
    HRESULT hr = S_OK;

    IfFailedReturn(ConvertException([&](){
        m_scriptName = pwszScriptName;
        IfFailedReturn(m_spScriptHostContext->NotifyScriptChange(this, ScriptRename));
        return S_OK;
    }));

    return hr;
}

HRESULT PythonScript::GetScriptFullFilePathName(_Out_ BSTR *pbstrScriptFullPathName)
{
    if (m_scriptFullPathName.empty())
    {
        *pbstrScriptFullPathName = nullptr;
    }
    else
    {
        *pbstrScriptFullPathName = SysAllocString(m_scriptFullPathName.c_str());
        if (*pbstrScriptFullPathName == nullptr)
        {
            return E_OUTOFMEMORY;
        }
    }

    return S_OK;
}

HRESULT PythonScript::SetScriptFullFilePathName(_In_ PCWSTR scriptFullPathName)
{
    return ConvertException([&]() {
        m_scriptFullPathName = scriptFullPathName;
        return S_OK;
    });
}

HRESULT PythonScript::Populate(_In_ IStream *pContentStream)
{
    HRESULT hr = S_OK;
    BYTE buf[1024];

    m_scriptContent.clear();

    // Cache a copy
    bool fEOF = false;
    while (!fEOF)
    {
        ULONG bytesRead;
        hr = pContentStream->Read(buf, ARRAYSIZE(buf), &bytesRead);
        fEOF = (hr == S_FALSE);
        IfFailedReturn(hr);

        IfFailedReturn(ConvertException([&](){
            m_scriptContent.insert(m_scriptContent.end(), &buf[0], &buf[bytesRead]);
            return S_OK;
        }));
    }

    //
    // The file stream won't necessarily have a null terminator...
    //
    IfFailedReturn(ConvertException([&](){
        m_scriptContent.push_back(0);
        m_scriptContent.push_back(0);
        return S_OK;
    }));

    if (hr == S_FALSE)
    {
        hr = S_OK;
    }

    //
    // The stream which is passed to us must be over UTF-16 data.
    //
    assert(m_scriptContent.size() % 2 == 0);

    //
    // Populated indicates we have no presently "executed" content and bridged namespace.  Repopulated indicates
    // that we do but an un-executed update has been pushed to the script.
    //
    m_state = (m_state == ScriptState::Executed ? ScriptState::Repopulated : ScriptState::Populated);
    return hr;
}

HRESULT PythonScript::InternalExecute()
{
    HRESULT hr = S_OK;

    //
    // We must preserve *ALL* prior executed content and the old script context until *EVERYTHING* succeeds.
    // Once that happens, we can swap out and destroy the old context.
    //
    // In order to do that, we create a full new script state and new script context.  The script must point
    // to the newly created state during initalization.  If committing the initialization fails, the state 
    // changes made to the object model as a result of the script state executing are undone and the newly
    // created state is no longer active.
    //
    assert(m_spActiveState.Get() == nullptr);
    IfFailedReturn(MakeAndInitialize<PythonScriptState>(&m_spActiveState, this, m_scriptContent, m_scriptFullPathName.c_str()));

    if (FAILED(hr = m_spActiveState->Execute()) ||
        FAILED(hr = m_spActiveState->InitializeScript()) ||
        FAILED(hr = m_spActiveState->FinalizeInitialization()))
    {
        //
        // Roll back the commit of this script.  Note that the destructor of the state
        // may cause a cascade of object model manipulations which undo the operations 
        // performed above.
        //
        m_spActiveState = nullptr;
    }

    return hr;
}

HRESULT PythonScript::Execute(_In_ IDataModelScriptClient *pScriptClient)
{
    HRESULT hr = S_OK;

    //
    // If we are just unlinked, we do not need to execute the root level code again.  We can simply
    // reinitialize the script.
    //
    if (m_state == ScriptState::Populated || 
        m_state == ScriptState::Repopulated ||
        m_state == ScriptState::Unlinked)
    {
        //
        // Preserve everything we can about the current state.  If a failure occurs anywhere in InternalExecute,
        // there is no active state
        //
        ComPtr<PythonScriptState> spCurrentState = std::move(m_spActiveState);

        //
        // If we have already executed script content prior to a new populate, we must uninitialize and delink
        // (otherwise initialization of the new script content may rightly fail).  Should the execution of new 
        // content fail, we will "roll back" by re-initializing the old content.  We do not need to actually
        // reinvoke the script or recreate the bridge objects.  Failure to execute should have prevented that!
        // 
        if (spCurrentState != nullptr)
        {
            hr = spCurrentState->UninitializeScript();
        }

        m_spReportingClient = pScriptClient;

        //
        // If we Execute after an Unlink, transition through Populated.
        //
        if (m_state == ScriptState::Unlinked)
        {
            m_state = ScriptState::Populated;
        }

        if (SUCCEEDED(hr))
        {
            hr = InternalExecute();
            if (FAILED(hr))
            {
                m_spActiveState = std::move(spCurrentState);

                //
                // If we failed (unless we're out-of-memory of some fatal error), all of the old bridges should still
                // be resident.  We just need to reinitialize and rebuild all the linkages.
                //
                if (m_spActiveState != nullptr)
                {
                    HRESULT hrReinitialize = m_spActiveState->InitializeScript();
                    if (FAILED(hrReinitialize))
                    {
                        hr = hrReinitialize;
                    }
                }
            }
        }

        m_spReportingClient = nullptr;
    }
    else
    {
        //
        // The script is either unpopulated or was already executed and no new script content is available for
        // re-execution.
        //
        hr = E_UNEXPECTED;
    }

    if (SUCCEEDED(hr))
    {
        m_state = ScriptState::Executed;
    }

    return hr;
}

HRESULT PythonScript::IsInvocable(_Out_ bool *pIsInvocable)
{
    if (m_state == ScriptState::Executed || m_state == ScriptState::Repopulated)
    {
        *pIsInvocable = m_spActiveState->HasMainFunction();
    }
    else
    {
        return E_FAIL;
    }

    return S_OK;
}

HRESULT PythonScriptState::ProcessBridgeElement(_In_ PyObject *pPyBridgeElement)
{
    HRESULT hr = S_OK;

    //
    // Create a registration record and apply such record.  If we cannot record the registration, undo
    // every change which was made to the object model on the basis of this record.
    //
    bool recordActive;
    PythonHostRegistration hostRegistration;
    IfFailedReturn(hostRegistration.Initialize(this, pPyBridgeElement));
    IfFailedReturn(hostRegistration.Apply(&recordActive));

    if (recordActive)
    {
        hr = ConvertException([&](){
            m_activeRegistrations.push_back(std::move(hostRegistration));
            return S_OK;
        });
    }

    if (FAILED(hr))
    {
        (void)hostRegistration.Undo();
    }

    return hr;
}

HRESULT PythonScriptState::InitializationBridge(_In_ PyObject *pPyInitList)
{
    HRESULT hr = S_OK;

    //
    // Iterate through the returned initialization list and set up the bridge for it.
    //
    auto pyIter = PinnedReference::Take(PyObject_GetIter(pPyInitList));
    IfObjectErrorConvertAndReturn(pyIter);

    while (SUCCEEDED(hr))
    {
        PinnedReference pyBridgeElement = PinnedReference::Take(PyIter_Next(pyIter));
        if (pyBridgeElement == nullptr)
        {
            //
            // PyIter_Next will return null with no exception set at EOL.  Failure will be null with
            // an exception set.
            //
            if (PyErr_Occurred())
            {
                hr = E_FAIL;            // @TODO: Convert...?
            }
            break;
        }

        hr = ProcessBridgeElement(pyBridgeElement);
    }

    //
    // If we failed part way through creating all the links to the script, we **MUST** unroll any changes
    // which were made from earlier registration records that are active!
    //
    if (FAILED(hr))
    {
        for(auto&& registration : m_activeRegistrations)
        {
            (void)registration.Undo();
        }
        m_activeRegistrations.clear();
    }

    return hr;
}

HRESULT PythonScriptState::InitializeScript()
{
    HRESULT hr = S_OK;
    auto switcher = EnterScript();

    Marshal::PythonMarshaler* pMarshaler = m_spScript->GetMarshaler();

    //
    // If the script exposes an "initializeScript" method, call it.  This is the core initializer.  Anything returned
    // from this must be interpreted as library objects which indicate the *AUTO* bridging between type signatures,
    // data models, and Python objects.
    //
    // Such bridging is managed by this provider and not explicitly controlled via imperative method calls in
    // the script itself.
    //
    if (PyObject_HasAttrString(GetModule(), "initializeScript"))
    {
        //
        // initializeScript *MUST* be a callable method!
        //
        PyObject* pInitializeScript = PyObject_GetAttrString(GetModule(), "initializeScript");
        if (!PyCallable_Check(pInitializeScript))
        {
            // (void)m_spScript->ReportError(ErrorClassError, E_FAIL, IDS_MUSTBE_FUNCTION, L"initializeScript");
            return E_FAIL;
        }

        PyObject *pArgs = PyTuple_New(0);
        if (pArgs == nullptr)
        {
            return E_FAIL;
        }
        auto args = PinnedReference::Take(pArgs);

        PyObject *pResult = PyObject_Call(pInitializeScript, args, nullptr);
        if (pResult == nullptr)
        {
            (void)m_spScript->ReportExceptionOrError(E_FAIL, &hr, ErrorClassError, IDS_FAIL_METHOD, L"initializeScript");
            assert(FAILED(hr));
            return hr;
        }

        assert(m_state == ScriptStateState::Executed || m_state == ScriptStateState::Inactive);
        m_state = ScriptStateState::UserInitialized;

        //
        // The return value from initializeScript indicates what bridging we need to perform to the object
        // model.  Run through this return.
        //
        if (!Py_IsNone(pResult))
        {
            IfFailedReturn(InitializationBridge(pResult));
        }
    }

    //
    // If the script exposes an "invokeScript" method, cache it.  This is the main script function.  If a client
    // asks us to invoke the main script method, this will get called.
    //
    if (PyObject_HasAttrString(GetModule(), "invokeScript"))
    {
        PyObject* pInvokeScript = PyObject_GetAttrString(GetModule(), "invokeScript");
        IfObjectErrorConvertAndReturn(pInvokeScript);

        if (!PyCallable_Check(pInvokeScript))
        {
            // (void)m_spScript->ReportError(ErrorClassError, E_FAIL, IDS_MUSTBE_FUNCTION, L"invokeScript");
            return E_FAIL;
        }

        m_pythonMainFunction = pInvokeScript;
    }

    //
    // Create a namespace object which can be added as a parent model to the actual namespace.  This is swapped
    // out only once everything in the script executes successfully.  Note that anything added as a parent model
    // must implement the data model concept (even if every such operation is a nop).
    //
    Object marshaledNamespace;
    Metadata marshaledMetadata;

    //
    // Marshal out the global object (we need to add additional filters to get rid of the core Script* routines)
    // and add it.
    //
    // Note that the reason this is done on a "per initialize" basis rather than a "per execute" basis is that the
    // marshaled object has a strong reference back to us in order to keep alive this entire script state for
    // anyone who has kept a live ref into the script.
    //
    // Us keeping a persistent pointer to the marshaled object would result in a reference loop that could not be broken.
    // The same is not true of the global object.  It cannot get back to us during an initialize.  If another method does
    // via caching, it *MUST* destroy its caches on an UninitializeScript.
    //
    IfFailedReturn(pMarshaler->MarshalFromPython(m_pModule, &marshaledNamespace, &marshaledMetadata, true, true));

    // 
    // Link the marshaled namespace object to the actual namespace.
    //
    Object& actualNamespace = m_spScript->GetHostNamespace();
    IfFailedReturn(actualNamespace->AddParentModel(marshaledNamespace, nullptr, false));
    m_namespaceObject = std::move(marshaledNamespace);

    m_state = ScriptStateState::Active;
    return hr;
}

HRESULT PythonScriptState::FinalizeInitialization()
{
    HRESULT hr = S_OK;
    auto switcher = EnterScript();
    // IfFailedReturn(m_spPythonLibrary->GetHostLibrary()->PhaseTwoInitialization());
    return hr;
}

HRESULT PythonScriptState::RuntimeClassInitialize(_In_ PythonScript *pScript,
                                                  _In_ std::vector<BYTE> const& scriptContent,
                                                  _In_opt_ PCWSTR scriptFullPathName)
{
    HRESULT hr = S_OK;

    auto lock = GlobalInterpreterLock::Lock();

#if 0
    //
    // @TODO: WEAKREF: initialize our weak ref...
    //
#endif // 0

    m_state = ScriptStateState::Created;
    m_spScript = pScript;
    PythonProvider *pProvider = pScript->GetProvider();

    IfFailedReturn(ConvertException([&](){
        int sz = WideCharToMultiByte(CP_UTF8,
                                     0,
                                     reinterpret_cast<LPCWCH>(&scriptContent[0]),
                                     scriptContent.size() / sizeof(wchar_t),
                                     nullptr,
                                     0,
                                     nullptr,
                                     nullptr);

        m_scriptContent.resize(sz + 1);
        int rsz = WideCharToMultiByte(CP_UTF8,
                                      0,
                                     reinterpret_cast<LPCWCH>(&scriptContent[0]),
                                     scriptContent.size() / sizeof(wchar_t),
                                     reinterpret_cast<LPSTR>(&(m_scriptContent[0])),
                                     sz,
                                     nullptr,
                                     nullptr);

        if (sz != rsz)
        {
            return E_FAIL;
        }

        m_scriptContent[sz] = 0;
        return S_OK;
    }));

    m_pModule = PyImport_AddModule("__foo__");
    if (m_pModule == nullptr)
    {
        return E_FAIL;
    }

    m_spPythonLibrary.reset(new(std::nothrow) Library::PythonLibrary());
    if (m_spPythonLibrary == nullptr)
    {
        return E_OUTOFMEMORY;
    }

    //
    // Initialization of the library will perform phase one initialization of the host library which
    // will bridge minimal support APIs into the script context.
    //
    // After all the root code has run and everything else is bridged, the full support API set can
    // be bridged with a PhaseTwoInitialize() call on the host library.
    //
    IfFailedReturn(m_spPythonLibrary->Initialize(m_pModule, scriptFullPathName));

    return hr;
}

ScriptSwitcher PythonScriptState::EnterScript()
{
    auto pMarshaler = m_spScript->GetMarshaler();
    ScriptSwitcher sw(pMarshaler, this);

    assert(!PyErr_Occurred());

    //
    // @TODO: debugger...  asynchronous break on entry...
    //

    return std::move(sw);
}

HRESULT PythonScriptState::Execute()
{
    HRESULT hr = S_OK;

    auto lock = GlobalInterpreterLock::Lock();
    PyObject *pDict = PyModule_GetDict(m_pModule);

#if 0
// @TODO: REMOVE THIS!
    ComPtr<Functions::PythonHostLibrary_DebugLog> spDebugLog;
    IfFailedReturn(MakeAndInitialize<Functions::PythonHostLibrary_DebugLog>(&spDebugLog, nullptr));
    IfFailedReturn(spDebugLog->AddToObject(m_pModule));
    spDebugLog.Detach();
// @TODO: REMOVE THIS!
#endif // 0

    PyObject *pValue = PyRun_String(reinterpret_cast<const char *>(&m_scriptContent[0]), 
                                    Py_file_input, 
                                    pDict, 
                                    pDict);

    if (pValue == nullptr)
    {
        (void)m_spScript->ReportExceptionOrError(E_FAIL, &hr, ErrorClassError, IDS_FAIL_EXECUTE);
        assert(FAILED(hr));
        return hr;
    }

    assert(m_state == ScriptStateState::Created);
    m_state = ScriptStateState::Executed;

    // @TODO: debugger: unpause...

    return hr;
}

} // Debugger::DataModel::ScriptProvider::Python

