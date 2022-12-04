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
};


} // Debugger::DataModel::ScriptProvider::Python::Functions
