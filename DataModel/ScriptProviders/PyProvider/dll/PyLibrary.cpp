//**************************************************************************
//
// PyLibrary.cpp
//
// Core support for standard Python methods and properties.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#include "PyProvider.h"

namespace Debugger::DataModel::ScriptProvider::Python::Library
{

HRESULT PythonLibrary::FindAttribute(_In_ PyObject *pPyObject,
                                     _In_ PCSTR pszAttribute,
                                     _Outptr_ PyObject **ppAttrResult)
{
    HRESULT hr = S_OK;
    *ppAttrResult = nullptr;

    //
    // Unfortunately, there is no PyObject_Get* API which will magically do this.  They will all
    // resolve any descriptor objects on the way out.  In order to do this, we need to manually
    // walk the MRO and ask each object in turn via a dictionary lookup.  This is less efficient
    // that Python can do because the attribute name will get repeatedly rehashed.  Alas...
    //
    // Do the **MINIMAL** amount of work necessary here.  Do **NOT** fetch the MRO chain unless
    // pPyObject itself does **NOT** have the attribute, etc...  This routine will be called incredibly
    // frequently for any visualizers or attached models.

    // Bear in mind that any object in the MRO walk might be a DMSO (e.g.: the Python class derives
    // from something outside of Python) and that needs to be very specially handled because the
    // Python dictionary **WILL NOT** contain the proxied attributes (at present). 
    //
    PinnedReference mro;
    bool fetchedMro = false;
    Py_ssize_t cur = 0;
    Py_ssize_t mroSize = 0;

    PyObject *pCur = pPyObject;
    for(;;)
    {
        PyObject *pDict = PyObject_GenericGetDict(pPyObject, nullptr);
        IfObjectErrorConvertAndReturn(pDict);
        auto dict = PinnedReference::Take(pDict);

        //
        // NOTE: PyDict_GetItemString returns a **BORROWED** reference.
        //
        PyObject *pItem = PyDict_GetItemString(dict, pszAttribute);
        if (pItem != nullptr)
        {
            Py_INCREF(pItem);
            *ppAttrResult = pItem;
            return S_OK;
        }

        //
        // If we haven't yet gotten the MRO, fetch it now.  This should only be done if the attribute
        // isn't immediately on the first object.
        //
        if (!fetchedMro)
        {
            assert(pCur == pPyObject);
            fetchedMro = true;

            if (PyObject_HasAttrString(pPyObject, "__mro__"))
            {
                mro = PyObject_GetAttrString(pPyObject, "__mro__");
                IfObjectErrorConvertAndReturn(mro);
                mroSize = PyTuple_Size(mro);
            }
        }

        if (cur >= mroSize)
        {
            break;
        }

        //
        // Move to the next type in MRO order and go ask it.
        //
        // NOTE: PyTuple_GetItem returns a borrowed reference...  which is exactly what we want 
        //       here since we have zero expectation that pCur holds an explicit GC ref.
        //
        pCur = PyTuple_GetItem(mro, cur);
        ++cur;
    }

    //
    // We could not find the attribute.
    //
    return S_FALSE;
}

HRESULT PythonLibrary::Initialize(_In_ PyObject *pModule, _In_opt_ PCWSTR scriptFullPathName)
{
    HRESULT hr = S_OK;
    Marshal::PythonMarshaler *pMarshaler = PythonProvider::Get()->GetMarshaler();

    m_pythonModule = pModule;

    // APIVERSION:
    //
    // The initial API version which is set up in the library is the 1.0 version.  Anything else must
    // be in ::InitializeVersion(...).
    //
    // Note that this means that functionally initializeScript is locked at the version 1.0 API unless there
    // is some other means of detecting.  Given that this is more about projected APIs and semantic changes,
    // it is unlikely that this will matter.
    //
    // New APIs do *NOT* need to be restricted by version.
    //
    m_apiVersionMajor = 1;                          // Current version in use is 1.0
    m_apiVersionMinor = 0;
    m_apiVersionMajorMax = 1;                       // Highest defined version is 1.0
    m_apiVersionMinorMax = 0;

    if (scriptFullPathName != nullptr)
    {
        m_scriptFullPathName = scriptFullPathName;
    }

    m_spHostLibrary.reset(new(std::nothrow) HostLibrary(this));
    if (m_spHostLibrary == nullptr)
    {
        return E_OUTOFMEMORY;
    }
    IfFailedReturn(m_spHostLibrary->PhaseOneInitialize());

    return hr;
}

} // Debugger::DataModel::ScriptProvider::Python::Library
