//**************************************************************************
//
// PyFunctions.cpp
//
// General support for (and implementations of) Python callable functions.
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

namespace Debugger::DataModel::ScriptProvider::Python::Functions
{

//*************************************************
// Fucntion Intrastructure:
//

PythonFunction::~PythonFunction()
{
}

HRESULT PythonFunction::InternalInitialize(_In_ const char *pName, 
                                           _In_ int flags,
                                           _In_ Marshal::PythonMarshaler *pMarshaler)
{
    HRESULT hr = S_OK;
    m_pMarshaler = pMarshaler;

    //
    // It's either a varargs or kwargs style function.  If one of these two flags isn't passed, assert and
    // bail out.  
    //
    void *pFunction = nullptr;
    if ((flags & METH_KEYWORDS) != 0)
    {
        pFunction = &(PythonFunction::call_vakw);
    }
    else if ((flags & METH_VARARGS) != 0)
    {
        pFunction = &(PythonFunction::call_va);
    }
    else
    {
        assert(false);
        return E_INVALIDARG;
    }

    DefineMethod(pName, pFunction, flags);
    PyObject *pCapsule = PyCapsule_New(reinterpret_cast<void *>(this),
                                       nullptr,
                                       nullptr);
    if (pCapsule == nullptr)
    {
        return E_FAIL;
    }

    m_pFunction = PyCFunction_NewEx(&m_methodDef, pCapsule, nullptr /*module*/);
    if (m_pFunction == nullptr)
    {
        return E_FAIL;
    }

    return hr;
}

HRESULT PythonFunction::AddToObject(_In_ PyObject *pObject)
{
    int result = PyObject_SetAttrString(pObject, GetName(), GetFunctionObject());
    if (result == -1)
    {
        PyObject *pType, *pValue, *pTraceback;
        PyErr_Fetch(&pType, &pValue, &pTraceback);

        const char *pStr = PyUnicode_AsUTF8AndSize(pValue, nullptr);

        PyErr_Restore(pType, pValue, pTraceback);
        // @TODO: Exception
        return E_FAIL;
    }
    return S_OK;
}

} // Debugger::DataModel::ScriptProvider::Python::Functions
