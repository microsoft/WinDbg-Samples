//**************************************************************************
//
// HostLibrary.h
//
// Core support for library routines projected by the host into the Python
// namespace.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

namespace Debugger::DataModel::ScriptProvider::Python::Functions
{

// PythonHostLibrary_ClassObject
//
// A dynamically created class.
//

// PythonHostLibrary_DebugLog 
//
// The impl class for the function host.diagnostics.debugLog(...)
//
class PythonHostLibrary_DebugLog : public PythonVaFunction
{
public:

    //*************************************************
    // Bridged Python APIs:
    //

    // InvokeVa():
    //
    // args: debugLog(object...)
    //
    virtual PyObject *InvokeVa(_In_ PyObject * /*pArgs*/) override;

    //*************************************************
    // Internal APIs:
    //

    HRESULT RuntimeClassInitialize(_In_ Marshal::PythonMarshaler *pMarshaler)
    {
        return BaseInitialize("debugLog", pMarshaler);
    }

private:

};

} // Debugger::DataModel::ScriptProvider::Python::Functions

namespace Debugger::DataModel::ScriptProvider::Python::Library
{

using namespace Debugger::DataModel::ScriptProvider::Python::Functions;
using namespace Debugger::DataModel::ScriptProvider::Python::Classes;

// HostLibrary:
//
// The host support library for the Python provider.
//
class HostLibrary
{
public:

    // HostLibrary():
    //
    // Construct the host library.
    //
    HostLibrary(_In_ PythonLibrary *pPythonLibrary) :
        m_pPythonLibrary(pPythonLibrary)
    {
    }

    //*************************************************
    // Internal APIs:
    //

    // PhaseOneInitialize():
    //
    // Initializes the "minimal level" of the host support library within a script context necessary
    // to enable the execution of global code and run the InitializeScript() method.
    //
    HRESULT PhaseOneInitialize();

    // PhaseTwoInitialize():
    //
    // Finishes the initialization of the host support library necessary to enable execution of arbitrary
    // code, methods, and property fetches.
    //
    HRESULT PhaseTwoInitialize();

private:

    // InitializeClass():
    //
    // Initializes and sets up one of our classes placing the class within the given namespace.
    //
    template<typename TClass, typename... TArgs>
    HRESULT InitializeClass(_In_opt_ PyObject *pNamespace,
                            _Out_ std::unique_ptr<TClass> *pspClass, 
                            _In_ TArgs&&... args)
    {
        HRESULT hr = S_OK;
        *pspClass = {};

        std::unique_ptr<TClass> spClass(new(std::nothrow) TClass);
        if (spClass == nullptr)
        {
            return E_OUTOFMEMORY;
        }

        IfFailedReturn(spClass->Initialize(std::forward<TArgs>(args)...));

        if (pNamespace != nullptr)
        {
            IfFailedReturn(spClass->AddToObject(pNamespace));
        }
        *pspClass = std::move(spClass);
        return hr;
    }

    //*************************************************
    // Namespaces / Functions
    //

    // host.diagnostics
    PinnedReference m_diagnosticsObject;

    // host.memory
    PinnedReference m_memoryObject;

    //*************************************************
    // Class Objects:
    //

    std::unique_ptr<PythonNamespace> m_spClass_Namespace;
    std::unique_ptr<PythonTypeSignatureRegistration> m_spClass_TypeSignatureRegistration;

    //*************************************************
    // Other Objects:
    //

    // Weak pointer back to the Python support library for the script context
    PythonLibrary *m_pPythonLibrary;

    // "host" which is inserted into the global namespace of Python.
    PinnedReference m_hostObject;

};

} // Debugger::DataModel::ScriptProvider::Python::Library
