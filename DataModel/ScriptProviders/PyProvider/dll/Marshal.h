//**************************************************************************
// 
// Marshal.h
//
// Marshaling constructs between Python objects and data model objects.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

namespace Debugger::DataModel::ScriptProvider::Python::Marshal
{

using namespace Debugger::DataModel::ClientEx;

//*************************************************
// Marshaling Objects Out Of Python:
//

// PythonSourceObject:
//
// Represents a Python object (or function) which was marshaled into the data model.  Note
// that not every interface supported by PythonSourceObject is necessarily "on" every marsahled
// object.
//
class PythonSourceObject :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        IDynamicKeyProviderConcept,
        IDynamicConceptProviderConcept,
        IDataModelConcept,
        IModelMethod
        >
{
public:

    // ~PythonSourceObject():
    //
    // The destructor for a source object.
    //
    virtual ~PythonSourceObject();

    //*************************************************
    // IDynamicKeyProviderConcept:
    //

    // GetKey():
    //
    // Called in order to fetch a key from the Python object.
    //
    IFACEMETHOD(GetKey)(_In_ IModelObject *pContextObject,
                        _In_ PCWSTR pwszKey,
                        _COM_Outptr_opt_result_maybenull_ IModelObject **ppKeyValue,
                        _COM_Outptr_opt_result_maybenull_ IKeyStore **ppMetadata,
                        _Out_opt_ bool *pHasKey);

    // SetKey():
    //
    // Called in order to set a key on the Python object.
    //
    IFACEMETHOD(SetKey)(_In_ IModelObject *pContextObject,
                        _In_ PCWSTR pwszKey,
                        _In_ IModelObject *pKeyValue,
                        _In_opt_ IKeyStore *pMetadata)
    {
        return E_NOTIMPL;
    }

    // EnumerateKeys():
    //
    // Called in order to enumerate all the keys on the Python object.
    //
    IFACEMETHOD(EnumerateKeys)(_In_ IModelObject *pContextObject,
                               _COM_Outptr_ IKeyEnumerator **ppEnumerator);

    //*************************************************
    // IDynamicConceptProviderConcept:
    //

    // GetConcept():
    //
    // Called in order to fetch a concept from the Python object.
    //
    IFACEMETHOD(GetConcept)(_In_ IModelObject *pContextObject,
                            _In_ REFIID conceptId,
                            _COM_Outptr_result_maybenull_ IUnknown **pConceptInterface,
                            _COM_Outptr_opt_result_maybenull_ IKeyStore **ppConceptMetadata,
                            _Out_ bool *pHasConcept);

    // SetConcept():
    //
    // Called in order to set a concept on the Python object.
    //
    IFACEMETHOD(SetConcept)(_In_ IModelObject *pContextObject,
                            _In_ REFIID conceptId,
                            _In_ IUnknown *pConceptInterface,
                            _In_opt_ IKeyStore *pConceptMetadata);

    // NotifyParent():
    //
    // 
    IFACEMETHOD(NotifyParent)(_In_ IModelObject *pParentModel);

    // NotifyParentChange():
    //
    //
    IFACEMETHOD(NotifyParentChange)(_In_ IModelObject *pParentModel);

    // NotifyDestruct():
    //
    // Notifies us of the destruction of the model object which housed the dynamic concept
    // provider. 
    //
    IFACEMETHOD(NotifyDestruct)();

    //*************************************************
    // IDataModelConcept:
    //
    
    // InitializeObject():
    //
    // Stub implementation.  We do not pre-cache antying on object construction.
    //
    IFACEMETHOD(InitializeObject)(_In_ IModelObject * /*pContextObject */,
                                  _In_opt_ IDebugHostTypeSignature * /*pMatchingTypeSignature*/,
                                  _In_opt_ IDebugHostSymbolEnumerator * /*pWildcardMatches*/)
    {
        return S_OK;
    }

    // GetName():
    //
    // Returns the name of the data model.
    //
    IFACEMETHOD(GetName)(_Out_ BSTR *pModelName);

    //*************************************************
    // IModelMethod:
    //

    // Call():
    //
    // Called from the data model side, this marshals the input arguments to Python, calls the underlying
    // function, and marshals the output argument back to the data model.
    //
    IFACEMETHOD(Call)(_In_opt_ IModelObject *pContextObject,
                      _In_ ULONG64 argCount,
                      _In_reads_(argCount) IModelObject **ppArguments,
                      _COM_Errorptr_ IModelObject **ppResult,
                      _COM_Outptr_opt_result_maybenull_ IKeyStore **ppMetadata);

    //*************************************************
    // Internal APIs:
    //

    // RuntimeClassInitialize():
    //
    // Initializes a new source object and creates a pinned reference back to the original
    // Python object.
    //
    HRESULT RuntimeClassInitialize(_In_ PyObject *pPyObject,
                                   _In_ PythonScriptState *pScriptState,
                                   _In_ bool isDataModel,
                                   _In_ bool isGlobalObject);

    // GetScriptState():
    //
    // Returns the script state to which this object belongs.
    //
    PythonScriptState *GetScriptState() const
    {
        return m_spOwningScriptState.Get();
    }

    // GetObject():
    //
    // Gets the Python object that this object represents.
    //
    PyObject *GetObject() const
    {
        return (PyObject *)m_pythonObject;
    }

    // IsGlobalObject():
    //
    // Indicates whether this source object is the global object.
    //
    bool IsGlobalObject() const
    {
        return m_isGlobalObject;
    }

    // GetMarshaler():
    //
    // Gets the marshaler.
    //
    PythonMarshaler *GetMarshaler() const;

private:

    // The original Python object we marshaled (with a pinned reference)
    PinnedReference m_pythonObject;

    // Back pointer to the script which owns this object.
    Microsoft::WRL::ComPtr<PythonScriptState> m_spOwningScriptState;


    // An indication of whether this object is a data model or not.  Objects registered as data models
    // can have a hierarchy outside what Python can see and we must be extraordinarily 
    // careful about what happens there.
    //
    bool m_isDataModel;
    bool m_isGlobalObject;

};

// PythonKeyEnumerator:
//
// An enumerator which walks the Python side of the object and returns an enumeration
// of everything available to the data model.
//
class PythonKeyEnumerator :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        IKeyEnumerator
        >
{
public:

    // ~PythonKeyEnumerator():
    //
    // Destructor for the key enumerator.
    //
    virtual ~PythonKeyEnumerator();

    //*************************************************
    // IKeyEnumerator:
    //

    // Reset():
    //
    // Resets the enumerator.
    //
    IFACEMETHOD(Reset)();

    // GetNext():
    //
    // Gets the next key and value from the enumerator.
    //
    IFACEMETHOD(GetNext)(_Out_ BSTR *pKeyName,
                         _COM_Errorptr_opt_ IModelObject **ppValue,
                         _COM_Outptr_opt_result_maybenull_ IKeyStore **ppMetadata);

    //*************************************************
    // Internal APIs:
    //

    // RuntimeClassInitialize():
    //
    // Initializes the key enumerator.
    //
    HRESULT RuntimeClassInitialize(_In_ PythonSourceObject *pEnumSrc, _In_ PyObject *pPyThis);

private:

    // FetchProperties():
    //
    // Internal helper which fetches the array of property names available on the current traversal
    // location in the class hierarchy.
    //
    HRESULT FetchProperties();

    // FetchResolutionBases():
    // 
    // Internal helper which fetches the __mro__ property to linearize the list of base classes of the object
    // in MRO order.  This allows 
    //
    HRESULT FetchResolutionBases();

    // AdvanceMRO():
    //
    // Moves forward to the next base class in the linearized list of bases (according to the Python method resolution
    // order).  Note that if there is no next prototype, S_FALSE is returned and m_enumObject is set to nullptr.
    //
    HRESULT AdvanceMRO();

    // Source object being enumerated (this is the object itself)
    Microsoft::WRL::ComPtr<PythonSourceObject> m_spEnumSrc;

    // The "this" of the object being enumerated.
    PinnedReference m_pyThis;

    // Indicates whether the enumerated object is a class (rather than an instance)
    bool m_objIsClass;

    // The linearization of the base class hierarchy as determined by __mro__
    PinnedReference m_pyMro;
    Py_ssize_t m_mroCount;
    Py_ssize_t m_mroCur;

    // Everything below refers to a point in the prototype chain of the object represented by m_spEnumSrc
    PinnedReference m_pyEnumObj;
    PinnedReference m_pyEnumObjDict;

    PinnedReference m_pyEnumObjDictItemList;
    Py_ssize_t m_itemListCount;
    Py_ssize_t m_cur;
};

//*************************************************
// Marshaling Objects Into Python:
//

// DataModelSourceObject:
//
// Represents a data model object which was marshaled into Python.
//
class DataModelSourceObject
{
public:

    DataModelSourceObject()
    {
    }

    // ~DataModelSourceObject():
    //
    // Data model source object destructor.
    //
    ~DataModelSourceObject() { }

    //*************************************************
    // Internal APIs:
    //

    // Initialize():
    // 
    // Initializes the data model source object.
    //
    HRESULT Initialize(_In_ IModelObject *pModelObject);

    // GetObject():
    //
    // Returns the original object.
    //
    IModelObject *GetObject() const
    {
        return (IModelObject *)m_modelObject;
    }

    // FromPythonObject():
    //
    // Attempts to return the data model source object from a Python object which may or may not
    // be a data model source object.
    //
    static HRESULT FromPythonObject(_In_ PyObject *pObject, _Out_ DataModelSourceObject **ppSourceObject);

    // CreateInstance():
    //
    // Creates a new PyObject attached to a new DMSO.
    //
    static PyObject *CreateInstance(_In_ IModelObject *pModelObject, 
                                    _Outptr_opt_ DataModelSourceObject **ppSourceObject);

    // GetMarshaler():
    //
    // Gets the marshaler associated with this DMSO.
    //
    PythonMarshaler *GetMarshaler() const;

    // GetType():
    //
    // Gets the type object for a DMSO.
    //
    static PyTypeObject *GetType()
    {
        return &s_PyType;
    }

    // GetTypeAsObject():
    //
    // Gets the type object for a DMSO as a PyObject.
    //
    static PyObject *GetTypeAsObject()
    {
        return reinterpret_cast<PyObject *>(&s_PyType);
    }

    // StaticInitialize():
    //
    // Performs necessary type initialization for the DMSO type.
    //
    static int StaticInitialize()
    {
        return PyType_Ready(&s_PyType);
    }

private:

    // PyData:
    //
    // The POD data which is associated with the actual Python object representing
    // a DMSO.
    //
    struct PyData
    {
        PyObject_HEAD;
        DataModelSourceObject *Object;
    };

    //*************************************************
    // Instance Python Callbacks:
    //

    // GetAttrO():
    //
    // The instance implementation of the tp_getattro callback from Python.  Gets a named attribute.
    //
    PyObject *GetAttrO(_In_ PyObject *pAttr);

    //*************************************************
    // Static Python Callbacks:
    //

    // AsDMSO():
    //
    // Given a Python object **KNOWN** to be a DMSO, get the DMSO pointer.
    //
    static DataModelSourceObject *AsDMSO(_In_ PyObject *pPyObject)
    {
        return reinterpret_cast<PyData *>(pPyObject)->Object;
    }

    // TpDestruct():
    //
    // The tp_destruct callback from Python.  Destroys the DMSO.
    //
    static void TpDestruct(_In_ PyObject *pSelf)
    {
        PyData *pData = reinterpret_cast<PyData *>(pSelf);
        delete (pData->Object);
        pData->Object = nullptr;
    }

    // TpGetAttrO():
    //
    // The tp_getattro callback from Python.  Gets a named attribute.
    //
    static PyObject *TpGetAttrO(_In_ PyObject *pSelf, _In_ PyObject *pAttr)
    {
        return AsDMSO(pSelf)->GetAttrO(pAttr);
    }

    //*************************************************
    // Static Data
    //

    // PyType:
    //
    // The PyTypeObject which defines a DMSO type object.
    //
    static PyTypeObject s_PyType;

    //*************************************************
    // Instance Data
    //

    Object m_modelObject;
};

//*************************************************
// General Marshaler:
//

// PythonMarshaler:
//
// The class which performs object marshaling into and out of Python.
//
class PythonMarshaler
{
public:

    PythonMarshaler(_In_ PythonProvider *pProvider, _In_ IDataModelManager *pManager, _In_ IDataModelNameBinder *pNameBinder) :
        m_pProvider(pProvider),
        m_spManager(pManager),
        m_spNameBinder(pNameBinder)
    {
    }

    // Initialize():
    //
    // Initializes the marshaler.
    //
    HRESULT Initialize();

    // MarshalFromPython():
    //
    // Takes a Python PyObject and marshals it to an IModelObject wrapped in a ClientEx::Object
    // RAII wrapper.  If there is metadata associated with the object, it can optionally be obtained.
    //
    HRESULT MarshalFromPython(_In_ PyObject *pPyObject,
                              _Out_ Object *pModelObject,
                              _Out_opt_ Metadata *pObjectMetadata,
                              _In_ bool isDataModel = false,
                              _In_ bool isGlobalObject = false);


    // MarshalToPython():
    //
    // Takes an IModelObject and marshals it to a Python object.  If the object was referenced
    // from another (e.g.: the 'x' in 'x.y'), the source object can be passed as well.  This
    // is required for marshaling methods and other things which have an implicit 'this' argument.
    //
    HRESULT MarshalToPython(_In_opt_ IModelObject *pSrcObject,
                            _In_ IModelObject *pModelObject,
                            _Out_ PyObject **ppPyObject);

    // BindNameToValue():
    //
    // Uses the default name binder to bind a name 'pwszName' in the context of a given object.
    //
    HRESULT BindNameToValue(_In_ IModelObject *pModelObject,
                            _In_ PCWSTR pwszName, 
                            _Out_ Object *pValue, 
                            _Out_opt_ Metadata *pMetadata);

    // BindNameToReference():
    //
    // Uses the default name binder to bind a name 'pwszName' to a reference in the context of a given object.
    //
    HRESULT BindNameToReference(_In_ IModelObject *pModelObject,
                                _In_ PCWSTR pwszName,
                                _Out_ Object *pReference,
                                _Out_opt_ Metadata *pMetadata);

    // EnumerateValues():
    //
    // Returns an enumerator for all name/values on the object as indicated by the default name binder.
    //
    HRESULT EnumerateValues(_In_ IModelObject *pModelObject,
                            _COM_Outptr_ IKeyEnumerator **ppEnum);

    // ConvertPythonException():
    //
    // Converts the current exception on the Python runtime to a model error object.  If there is not a
    // current exception, the specific error passed in is utilized to create an output error.
    //
    HRESULT ConvertPythonException(_In_ HRESULT hrConverted,
                                   _Out_ Object *pErrorObject,
                                   _Out_ HRESULT *pResult);

    // CreatePythonObjectForModelObject():
    //
    // Creates a new Python object for the given model object.  The resulting Python object is an
    // instance of the DataModelSourceObject "Python type" and implements the necessary protocols to bridge
    // from Python back to the data model.
    //
    HRESULT CreatePythonObjectForModelObject(_In_opt_ IModelObject *pSourceObject,
                                             _In_ IModelObject *pModelObject,
                                             _Out_ PyObject **ppPyObject);

    // SetDataModelError():
    //
    // If there's not already an exception in flight on the runtime in question, set the data model error as the 
    // exception.
    //
    HRESULT SetDataModelError(_In_ HRESULT hrFail, _In_opt_ IModelObject *pErrorObject);

    // ScriptEntryType:
    //
    // Defines the type of state change.
    //
    enum class ScriptEntryType
    {
        ScriptEntry,
        ScriptExit,
        TemporaryChange
    };

    // SetActiveScriptState():
    //
    // Sets the currently active script state for the marshaler & provider.
    //
    HRESULT SetActiveScriptState(_In_opt_ PythonScriptState *pScriptState,
                                 _In_ ScriptEntryType entryType);

    // GetActiveSciptState():
    //
    // Gets the currently active script state.
    //
    PythonScriptState *GetActiveScriptState() const
    {
        return m_spActiveScriptState.Get();
    }

    // IsGlobalNameExcludedFromMarshaling():
    //
    // Returns whether or not a global name is excluded from marshaling across the marshaling
    // boundary into the data model.
    //
    bool IsGlobalNameExcludedFromMarshaling(_In_ PCSTR pszPropertyName, _In_ ULONG apiMajorVersion = 1, _In_ ULONG apiMinorVersion = 0)
    {
        return IsNameExcludedFromMarshaling(pszPropertyName, m_globalExclusionSet, apiMajorVersion, apiMinorVersion);
    }

    // IsObjectNameExcludedFromMarshaling():
    //
    // Returns whether or not an object name is excluded from marshaling across the marshaling
    // boundary into the data model.
    //
    bool IsObjectNameExcludedFromMarshaling(_In_ PCSTR pszPropertyName, _In_ ULONG apiMajorVersion = 1, _In_ ULONG apiMinorVersion = 0)
    {
        return IsNameExcludedFromMarshaling(pszPropertyName, m_exclusionSet, apiMajorVersion, apiMinorVersion);
    }

    // ModelValueToPython():
    //
    // Reads the value represented by the pModelObject and converts it to a Python value
    //
    PyObject *ModelValueToPython(_In_ IModelObject * pModelObject);

private:

    // ExclusionEntry:
    //
    // Defines a name that cannot marshal out of Python (because it is mapped to a protocol, etc...)
    // As new protocols are added, names can be specifically excluded on an Api version basis.
    //
    struct ExclusionEntry
    {
        ULONG ApiMajorVersion;
        ULONG ApiMinorVersion;
    };

    typedef std::unordered_map<std::string, ExclusionEntry> ExclusionSet;
    typedef typename ExclusionSet::value_type ExclusionSetEntry;

    // IsNameExcludedFromMarshaling():
    //
    // Returns whether or not a given name is excluded from marshaling across the boundary into the data
    // model.
    //
    bool IsNameExcludedFromMarshaling(_In_ PCSTR pszPropertyName, 
                                      _In_ ExclusionSet& exclusionSet,
                                      _In_ ULONG apiMajorVersion = 1,
                                      _In_ ULONG apiMinorVersion = 0)
    {
        bool found = false;

        //
        // Names which begin with '__' are considered reserved or internal names and do not get taken across
        // the marshaling boundary either.
        //
        if (pszPropertyName[0] == '_' && pszPropertyName[1] == '_')
        {
            found = true;
        }

        if (!found)
        {
            auto fn = [&](){
                auto it = exclusionSet.find(pszPropertyName);
                if (it != exclusionSet.end())
                {
                    if (apiMajorVersion > it->second.ApiMajorVersion || 
                        (apiMajorVersion == it->second.ApiMajorVersion && apiMinorVersion >= it->second.ApiMinorVersion))
                    {
                        found = true;
                    }
                }
                return S_OK;
            };
            ConvertException(fn);
        }

        return found;
    }

    Microsoft::WRL::ComPtr<IDataModelManager> m_spManager;
    Microsoft::WRL::ComPtr<IDataModelNameBinder> m_spNameBinder;

    // WEAK back pointer to the provider which owns us.
    PythonProvider *m_pProvider;

    //
    // The currently active script state.
    //
    Microsoft::WRL::ComPtr<PythonScriptState> m_spActiveScriptState;

    // The set of names excluded from property mapping across the marshaling boundary into the data model.
    ExclusionSet m_globalExclusionSet;
    ExclusionSet m_exclusionSet;
};

} // Debugger::DataModel::ScriptProvider::Python::Marshal
