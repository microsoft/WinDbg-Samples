//**************************************************************************
//
// HostLibrary.cpp
//
// Core support for library routines projected by the host into the Python namespace.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#include "PyProvider.h"

// @TODO: This is temporary.  See host.diagnostics.debugLog.  A proper channel should be established for this
#include <DbgEng.h>

using namespace Microsoft::WRL;
using namespace Debugger::DataModel;
using namespace Debugger::DataModel::ScriptProvider;
using namespace Debugger::DataModel::ScriptProvider::Python;

namespace Debugger::DataModel::ScriptProvider::Python::Functions
{

PyObject *PythonHostLibrary_DebugLog::InvokeVa(_In_ PyObject *pArgs)
{
    PythonProvider *pProvider = PythonProvider::Get();
    Marshal::PythonMarshaler *pMarshaler = GetMarshaler();

    //
    // @TODO: This **ABSOLUTELY** should not touch IDebugControl but should define a new channel for diagnostics
    //        information back to the host or script client.  For now, this will go to engine output to simply
    //        allow some level of "printf" style debugging.  This API is **NOT** intended for general printf output
    //        and makes NO GUARANTEE that the debug channel is such.  In actuality, this will probably go to some
    //        "log window" or the like on the client which is completely up to the client as to presentation.
    //
    ComPtr<IDebugHost> spHost;
    ComPtr<IUnknown> spPrivate;
    IfFailedThrow(pProvider->GetHostSymbols()->QueryInterface(IID_PPV_ARGS(&spHost)));
    IfFailedThrow(spHost->GetHostDefinedInterface(&spPrivate));

    ComPtr<IDebugControl4> spCtrl;
    IfFailedThrow(spPrivate.As(&spCtrl));

    std::string outputString;
    auto fn = [&]()
    {
        HRESULT hr = S_OK;

        Py_ssize_t argCount = PyTuple_Size(pArgs);
        for (Py_ssize_t i = 0; i < argCount; ++i)
        {
            PyObject *pArg = PyTuple_GetItem(pArgs, i);
            if (pArg == nullptr)
            {
                return E_UNEXPECTED;
            }

            PyObject *pStr;
            PinnedReference str;
            if (PyUnicode_Check(pArg))
            {
                pStr = pArg;
            }
            else
            {
                pStr = PyObject_Str(pArg);
                if (pStr == nullptr)
                {
                    return E_FAIL;
                }
                str = PinnedReference::Take(pStr);
            }

            const char *pUTF8Str = PyUnicode_AsUTF8AndSize(pStr, nullptr);
            outputString += pUTF8Str;
        }

        return hr;
    };

    IfFailedThrow(ConvertException(fn));
    IfFailedThrow(spCtrl->OutputWide(DEBUG_OUTPUT_NORMAL, L"%S", outputString.c_str()));

    Py_RETURN_NONE;
};

} // Debugger::DataModel::ScriptProvider::Python::Functions

namespace Debugger::DataModel::ScriptProvider::Python::Library
{

HRESULT HostLibrary::PhaseOneInitialize()
{
    HRESULT hr = S_OK;
    Marshal::PythonMarshaler *pMarshaler = PythonProvider::Get()->GetMarshaler();

    //
    // Set up our "namespace" class which is much like 'types.SimpleNamespace' but relies on no module
    // imports.  All things like 'host' and 'host.diagnostics' are instances of this class.
    //
    
    IfFailedReturn(InitializeClass<Classes::PythonNamespace>(nullptr, &m_spClass_Namespace, pMarshaler));

    m_hostObject = m_spClass_Namespace->CreateInstance();
    IfObjectErrorConvertAndReturn(m_hostObject);

    m_diagnosticsObject = m_spClass_Namespace->CreateInstance();
    IfObjectErrorConvertAndReturn(m_diagnosticsObject);

    //
    // Link any sub-namespaces.
    //
    if (PyObject_SetAttrString(m_hostObject, "diagnostics", m_diagnosticsObject) < 0) { return E_FAIL; }

    //
    // Set up key diagnostics fucntionality.
    //

    ComPtr<Functions::PythonFunctionTable> spDiagnosticsFunctions;
    IfFailedReturn(MakeAndInitialize<Functions::PythonFunctionTable>(&spDiagnosticsFunctions));
    IfFailedReturn(spDiagnosticsFunctions->NewFunction<Functions::PythonHostLibrary_DebugLog>(pMarshaler));
    IfFailedReturn(spDiagnosticsFunctions->AddToObject(m_diagnosticsObject));

    //
    // Set up key attributes of the 'host' library
    //

    IfFailedReturn(InitializeClass<Classes::PythonTypeSignatureRegistration>(m_hostObject, 
                                                                             &m_spClass_TypeSignatureRegistration, 
                                                                             pMarshaler));
    IfFailedReturn(ConvertException([&]() {
        m_classRegistrations.insert({ reinterpret_cast<ULONG_PTR>(m_spClass_TypeSignatureRegistration->GetClassObject()),
                                     HostRegistrationKind::TypeSignatureRegistration });
        return S_OK;
    }));

    //
    // Place the host in the global namespace of this script.  This only has a limited set of functionality until
    // PhaseTwoInitialize() finishes.
    //

    if (PyObject_SetAttrString(m_pPythonLibrary->GetModule(), "host", m_hostObject) < 0) { return E_FAIL; }
    return hr;
}

HRESULT HostLibrary::GetSignatureInformation(_In_ PyObject *pPyBridgeElement,
                                             _Out_ std::wstring *pTypeSignature,
                                             _Out_ std::wstring *pModuleName,
                                             _Out_ std::wstring *pMinVersion,
                                             _Out_ std::wstring *pMaxVersion,
                                             _Out_ PinnedReference *pClassRegistration)
{
    auto fn = [&]()
    {
        HRESULT hr = S_OK;
        *pTypeSignature = { };
        *pModuleName = { };
        *pMinVersion = { };
        *pMaxVersion = { };
        *pClassRegistration = { };

        auto classObject = PinnedReference::Take(PyObject_GetAttrString(pPyBridgeElement, "class_object"));
        IfObjectErrorConvertAndReturn(classObject);

        auto typeSignature = PinnedReference::Take(PyObject_GetAttrString(pPyBridgeElement, "type_signature"));
        IfObjectErrorConvertAndReturn(pTypeSignature);

        PinnedReference moduleInfo;
        if (PyObject_HasAttrString(pPyBridgeElement, "module_info"))
        {
            moduleInfo = PinnedReference::Take(PyObject_GetAttrString(pPyBridgeElement, "module_info"));
            IfObjectErrorConvertAndReturn(moduleInfo);
        }

        PinnedReference minVersion;
        if (PyObject_HasAttrString(pPyBridgeElement, "min_version"))
        {
            minVersion = PinnedReference::Take(PyObject_GetAttrString(pPyBridgeElement, "min_version"));
            IfObjectErrorConvertAndReturn(minVersion);
        }

        PinnedReference maxVersion;
        if (PyObject_HasAttrString(pPyBridgeElement, "max_version"))
        {
            maxVersion = PinnedReference::Take(PyObject_GetAttrString(pPyBridgeElement, "max_version"));
            IfObjectErrorConvertAndReturn(maxVersion);
        }

        const char *pTypeSignatureStr = PyUnicode_AsUTF8AndSize(typeSignature, nullptr);
        IfFailedReturn(GetUTF16(pTypeSignatureStr, pTypeSignature));

        //
        // @TODO: module objects...
        //
        if (moduleInfo)
        {
            if (PyUnicode_Check(moduleInfo))
            {
                const char *pModuleInfoStr = PyUnicode_AsUTF8AndSize(moduleInfo, nullptr);
                IfFailedReturn(GetUTF16(pModuleInfoStr, pTypeSignature));
            }
            else
            {
                return E_INVALIDARG;
            }
        }

        if (minVersion)
        {
            const char *pMinVersionStr = PyUnicode_AsUTF8AndSize(minVersion, nullptr);
            IfFailedReturn(GetUTF16(pMinVersionStr, pMinVersion));
        }

        if (maxVersion)
        {
            const char *pMaxVersionStr = PyUnicode_AsUTF8AndSize(maxVersion, nullptr);
            IfFailedReturn(GetUTF16(pMaxVersionStr, pMaxVersion));
        }

        *pClassRegistration = std::move(classObject);
        return hr;
    };
    return ConvertException(fn);
}
    
} // Debugger::DataModel::ScriptProvider::Python::Library


