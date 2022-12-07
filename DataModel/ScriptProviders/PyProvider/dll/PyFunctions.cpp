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
    if (m_pFunction != nullptr)
    {
        Py_DECREF(m_pFunction);
    }
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

    PinnedReference capsule = PinnedReference::Take(PyCapsule_New(reinterpret_cast<void *>(this),
                                                                  nullptr,
                                                                  &(PythonFunction::destruct)));
    IfObjectErrorConvertAndReturn(capsule);

    //
    // *** DANGER *** : After this line of code executes, we rely on the Python GC to collect this object.
    //                  The destruction of the capsule will invoke PythonFunction::destruct which will
    //                  release the corresponding reference.
    //
    //                  If the construction of the function fails, the capsule will destruct at the end of
    //                  this scope and it is expected that the capsule holds a single COM ref on the function!
    //
    AddRef();

    m_pFunction = PyCFunction_NewEx(&m_methodDef, capsule, nullptr /*module*/);
    IfObjectErrorConvertAndReturn(capsule);

    //
    // The capsule is the function entity and was created.  Let our hold on it go.  The Python GC owns this.
    //
    capsule.Detach();

    return hr;
}

HRESULT PythonFunction::AddToObject(_In_ PyObject *pObject)
{
    //
    // If we are explicitly being asked to add this to a dictionary, it's not an attribute...
    //
    int result = 0;
    if (PyDict_Check(pObject))
    {
        result = PyDict_SetItemString(pObject, GetName(), GetFunctionObject());
    }
    else
    {
        result = PyObject_SetAttrString(pObject, GetName(), GetFunctionObject());
    }
      
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
