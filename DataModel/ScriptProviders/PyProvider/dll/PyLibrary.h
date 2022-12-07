//**************************************************************************
//
// PyLibrary.h
//
// Core support for standard Python methods and properties.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

namespace Debugger::DataModel::ScriptProvider::Python::Library
{

// PythonLibrary:
//
// Represents the set of routines we must call into core Python in order to bridge
// between Python and the data model.
//
class PythonLibrary
{
public:

    // FindAttribute():
    //
    // Finds an attribute on the object in MRO order.  This is largely equivalent to the internal _PyType_Lookup
    // in that it will return a descriptor instead of resolving it.  It is up to the caller to deal with descriptors.
    //
    // Note that this should *ONLY* be called for cases where the "this" / "self" pointer is *NOT* the same as
    // the object being passed (e.g.: the Python class is a data model and not just an instance).  Let Python 
    // deal internally with the simple cases (and whatever optimizations it has).
    //
    HRESULT FindAttribute(_In_ PyObject *pPyObject,
                          _In_ PCSTR pszAttribute,
                          _Outptr_ PyObject **ppAttrResult);

    // SupportsApiVersion():
    //
    // Determines whether this library supports version major.minor of the PyProvider API.  Note that new APIs
    // can be added to an older version with no issue.  API changes, new *PROJECTED* APIs, and other source level
    // breaks require an explicit opt in through this mechanism.
    //
    bool SupportsApiVersion(_In_ ULONG apiVersionMajor,
                            _In_ ULONG apiVersionMinor = 0) const
    {
        return (m_apiVersionMajor > apiVersionMajor || (m_apiVersionMajor == apiVersionMajor && m_apiVersionMinor >= apiVersionMinor));
    }

    // GetApiVersion():
    //
    // Gets the major.minor version of the PyProvider API that the library supports.  Callers should prefer SupportsApiVersion
    // rather than fetching this.
    //
    void GetApiVersion(_Out_ ULONG *pMajorVersion, _Out_ ULONG *pMinorVersion) const
    {
        *pMajorVersion = m_apiVersionMajor;
        *pMinorVersion = m_apiVersionMinor;
    }

    //*************************************************
    // Internal APIs:
    //

    //
    // Initialize():
    //
    // Initializes all support routines that we must call into JavaScript for a particular script context.
    // Said script context must be the currently active script context.
    //
    HRESULT Initialize(_In_ PyObject *pModule, _In_opt_ PCWSTR scriptFullPathName);

    //
    // InitializeApiVersionSupport():
    //
    // Initializes support for API versions above 1.0 when indicated by the script that it has opted into such.
    //
    HRESULT InitializeApiVersionSupport(_In_ ULONG majorVersion, _In_ ULONG minorVersion);

    // GetHostLibrary():
    //
    // Gets the host support library.
    //
    HostLibrary* GetHostLibrary() const
    {
        return m_spHostLibrary.get();
    }

    // GetModule():
    //
    // Gets the "module" to which we belong.
    //
    PyObject* GetModule() const
    {
        return m_pythonModule;
    }

private:

    //*************************************************
    // Versioning
    //

    //
    // Which version of the PyProvider this library exposes.  Certain features (e.g.: new projected names)
    // are fundamentally source breaking.  A script needs to opt into a new "API Version" in order to get such
    // semantics.
    //
    // The initial PyProvider API version number is 1.0.
    //
    ULONG m_apiVersionMajor;
    ULONG m_apiVersionMinor;

    ULONG m_apiVersionMajorMax;
    ULONG m_apiVersionMinorMax;

    std::wstring m_scriptFullPathName;

    //*************************************************
    // Other Objects:
    //

    // The "module" to which we belong
    PinnedReference m_pythonModule;

    // The "host" support library
    std::unique_ptr<HostLibrary> m_spHostLibrary;

};

} // Debugger::DataModel::ScriptProvider::Python::Library
