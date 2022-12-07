//**************************************************************************
//
// PyClasses.h
//
// General support (and implementations of) Python classes
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

namespace Debugger::DataModel::ScriptProvider::Python::Classes
{

//**************************************************************************
// Class Infrastructure:
//

// PythonClass:
//
// Basic implementation of a Python class.  Clients should derive from this and not instantiate it directly.
// Note that if tp_alloc is overriden, the overriding class must hold the contract which inserts the class instance
// pointer in the basic size (e.g.: Data below)
//
class PythonClass
{
public:

    PythonClass(_In_ char const *pName, 
                _In_ unsigned int pyFlags = 0,
                _In_ int instanceDataSize = 0, 
                _In_ int itemDataSize = 0);

    virtual ~PythonClass();

    // GetName():
    //
    // Gets the name of the function.
    //
    const char *GetName() const
    {
        return m_classSpec.name;
    }

    // AddToObject():
    //
    // Adds this function into an object.
    //
    HRESULT AddToObject(_In_ PyObject *pObject);

    // GetClassObject():
    //
    // Gets the class (type) object.
    //
    PyObject *GetClassObject() const
    {
        assert(m_pClass != nullptr);
        return m_pClass;
    }

    // CreateInstance():
    //
    // Creates an instance of the class.
    //
    PyObject *CreateInstance(_In_ PyObject *pArgs = nullptr, _In_ PyObject *pKwArgs = nullptr);

protected:

    // Data:
    //
    // Our "instance data block".  Note that we *ALWAYS* carry a variable length header.  If the class
    // is fixed length, the ob_size field is simply considered alignment padding.  From Python's perspective,
    // it is part of our instance data.
    //
    struct Data
    {
        PyObject_VAR_HEAD
        PythonClass *Class;

        //
        // Client instance data follows.  Size is m_classSpec.basicsize - sizeof(Data)
        //

        //
        // Client item data follows.  Size is m_classSpec.itemsize * ob_size
        //
    };

    //*************************************************
    // Slot Implementations:
    // 
    // Any derived class can override these.  Note that they are not hooked up unless the derived class
    // makes an explicit call in its initializer.
    //

    virtual int TpInit(_In_ PyObject *pSelf, _In_ PyObject *pArgs, _In_ PyObject *pKwArgs) { return -1; }
    virtual void TpDealloc(_In_ PyObject *pSelf) { }
    virtual int TpTraverse(_In_ PyObject *pSelf, _In_ visitproc visit, _In_ void *arg) { return 0; }
    virtual PyObject* TpAlloc(_In_ PyTypeObject *pTypeObject, _In_ Py_ssize_t nItems);

    //*************************************************
    // Static Bridge Routines:
    //

    static int Bridge_TpInit(_In_ PyObject *pSelf, _In_ PyObject *pArgs, _In_ PyObject *pKwArgs)
    {
        return FromInstance(pSelf)->TpInit(pSelf, pArgs, pKwArgs);
    }

    static void Bridge_TpDealloc(_In_ PyObject *pSelf)
    {
        return FromInstance(pSelf)->TpDealloc(pSelf);
    }

    static int Bridge_TpTraverse(_In_ PyObject *pSelf, visitproc visit, void *arg)
    {
        return FromInstance(pSelf)->TpTraverse(pSelf, visit, arg);
    }

    static PyObject *Bridge_TpAlloc(_In_ PyTypeObject *pTypeObject, _In_ Py_ssize_t nItems)
    {
        return FromTypeObject(pTypeObject)->TpAlloc(pTypeObject, nItems);
    }

    //*************************************************
    // Internal Methods:
    //

    // FromInstance():
    //
    // Returns the PythonClass object associated with this instance.
    //
    static PythonClass *FromInstance(_In_ PyObject *pInstance)
    {
        Data *pData = reinterpret_cast<Data *>(pInstance);
        PythonClass *pClass = (pData->Class);

        assert(pClass != nullptr && pClass->m_pClass != nullptr && PyObject_IsInstance(pInstance, pClass->m_pClass));
        return (pData->Class);
    }

    // FromTypeObject():
    //
    // Returns the PythonClass object associated with the given type object.
    //
    static PythonClass *FromTypeObject(_In_ PyTypeObject *pClass);

    // InstanceData():
    //
    // Returns the instance data for a given object known to be an instance of this class.
    //
    void *InstanceData(_In_ PyObject *pInstance) const
    {
        assert(m_pClass != nullptr && PyObject_IsInstance(pInstance, m_pClass));
        char *pData = reinterpret_cast<char *>(pInstance) + sizeof(Data);
        return reinterpret_cast<void *>(pData);
    }

    // ItemDataN():
    //
    // Returns the Nth variable length item data for a given object known to be an instance of this class.
    // If the type is not variable length, this will return nullptr.  If N is outside the bounds of the variable
    // length data, this will return nullptr.
    //
    void *ItemDataN(_In_ PyObject *pInstance, _In_ Py_ssize_t n) const
    {
        assert(m_pClass != nullptr && PyObject_IsInstance(pInstance, m_pClass));
        if (m_classSpec.itemsize == 0) { return nullptr; }

        PyVarObject *pHead = reinterpret_cast<PyVarObject *>(pInstance);
        if (n >= pHead->ob_size) { return nullptr; }

        char *pData = reinterpret_cast<char *>(pInstance) + m_classSpec.basicsize;
        pData = pData + n * m_classSpec.itemsize;

        return reinterpret_cast<void *>(pData);
    }

    // InstanceDataAs():
    //
    // Returns the instance data for a given object known to be an instance of this class.  Such data is
    // returned as a typed pointer and *NOT* a void *.  The given type *MUST* be the size of the instance 
    // data.
    //
    template<typename TInstance>
    TInstance *InstanceDataAs(_In_ PyObject *pInstance) const
    {
        assert(sizeof(TInstance) == m_classSpec.basicsize - sizeof(Data));
        return reinterpret_cast<TInstance *>(InstanceData(pInstance));
    }

    // ItemDataNAs():
    //
    // As ItemDataN but returns a typed pointer.  The given type *MUST* be the size of the item data.
    //
    template<typename TItem>
    TItem *ItemDataNAs(_In_ PyObject *pInstance, _In_ Py_ssize_t n)
    {
        assert(sizeof(TItem) == m_classSpec.itemsize);
        return reinterpret_cast<TItem *>(ItemDataN(pInstance, n));
    }

    // ItemCount():
    //
    // Returns the number of items (variable length) within the instance.  If the type is not a variable
    // length type, this returns 0.
    //
    Py_ssize_t ItemCount(_In_ PyObject *pInstance) const
    {
        assert(m_pClass != nullptr && PyObject_IsInstance(pInstance, m_pClass));
        if (m_classSpec.itemsize == 0) { return 0; }

        PyVarObject *pHead = reinterpret_cast<PyVarObject *>(pInstance);
        return pHead->ob_size;
    }

    // Initialize():
    //
    // Finishes the initialization.  Note that the derived class must have called AddSlot with any slot implementations
    // *PRIOR* to calling this method!
    //
    HRESULT Initialize(_In_ Marshal::PythonMarshaler *pMarshaler);

    // AddSlot():
    //
    // Adds a new slot implementation.  This is hooked up to the appropriate virtual method in PythonClass.
    //
    HRESULT AddSlot(_In_ int slot);

    // AddSlots():
    //
    // Adds an arbitrary number of slot implementations.
    //
    HRESULT AddSlots(_In_ size_t count, ...)
    {
        HRESULT hr = S_OK;
        va_list va;
        va_start(va, count);
        for (size_t i = 0; i < count && SUCCEEDED(hr); ++i)
        {
            hr = AddSlot(va_arg(va, int));
        }
        return hr;
    }

    // HasDefinedSlot():
    //
    // Indicates whether a slot is defined for this class or not (in the class type spec).
    //
    bool HasDefinedSlot(_In_ int slot)
    {
        for (auto&& definedSlot : m_slots)
        {
            if (definedSlot.slot == slot && definedSlot.pfunc != nullptr)
            {
                return true;
            }
        }

        return false;
    }

    //*************************************************
    // Internal Data:
    //

    static constexpr size_t SlotCount = Py_am_send + 1;
    static PyType_Slot s_classSlots[SlotCount];
    static std::unordered_map<ULONG_PTR, PythonClass *> s_classMap;

    // The class specification
    PyType_Spec m_classSpec;

    // The class implementation slots
    std::vector<PyType_Slot> m_slots;

    // The actual class object
    PyObject *m_pClass;

    // Other data
    Marshal::PythonMarshaler *m_pMarshaler;

};

//**************************************************************************
// Individual Classes
//

// PythonNamesapce:
//
// A simple namespace that all of our internal namespaces are built atop.  This is somewhat
// equivalent to 'type.SimpleNamespace'
//
class PythonNamespace : public PythonClass
{
public:
    
    PythonNamespace() :
        PythonClass("Namespace", Py_TPFLAGS_MANAGED_DICT | Py_TPFLAGS_HAVE_GC)
    {
    }

    HRESULT Initialize(_In_ Marshal::PythonMarshaler *pMarshaler);

};

// PythonTypeSignatureRegistration:
//
// host.TypeSignatureRegistration class implementation
//
class PythonTypeSignatureRegistration : public PythonClass
{
public:

    PythonTypeSignatureRegistration() :
        PythonClass("TypeSignatureRegistration", Py_TPFLAGS_MANAGED_DICT | Py_TPFLAGS_HAVE_GC)
    {
    }

    HRESULT Initialize(_In_ Marshal::PythonMarshaler *pMarshaler);

protected:

    virtual int TpInit(_In_ PyObject *pSelf, _In_ PyObject *pArgs, _In_ PyObject *pKwArgs) override;

};

}
