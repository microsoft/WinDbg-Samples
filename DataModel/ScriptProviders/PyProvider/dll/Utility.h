//**************************************************************************
// 
// Utility.h
//
// General utilities for the Python provider.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

namespace Debugger::DataModel::ScriptProvider::Python
{

// VariantDeleter:
//
// A deletion functor which deletes a variant.
//
struct VariantDeleter
{
    void operator()(_In_ VARIANT *pv)
    {
        VariantClear(pv);
    }
};

// variant_ptr
//
// A unique_pointer over a VARIANT which deletes it with the appropriate VariantClear call.
//
using variant_ptr = std::unique_ptr<VARIANT, VariantDeleter>;

// GlobalInterpreterLock:
//
// Represents an acquisition of the Python GIL.
//
class GlobalInterpreterLock
{
public:

    ~GlobalInterpreterLock()
    {
        PyGILState_Release(m_gstate);
    }

    static GlobalInterpreterLock Lock()
    {
        return GlobalInterpreterLock();
    }

private:

    GlobalInterpreterLock()
    {
        m_gstate = PyGILState_Ensure();
    }

    PyGILState_STATE m_gstate;

};

// ScriptSwitcher:
//
// Switches the current script and switches it back.
//
class ScriptSwitcher
{
public:

    ~ScriptSwitcher();

    ScriptSwitcher(_In_ Marshal::PythonMarshaler *pMarshaler,
                   _In_ PythonScriptState *pScriptState);

    ScriptSwitcher(ScriptSwitcher&& src) :
        m_pMarshaler(src.m_pMarshaler),
        m_spPriorScriptState(src.m_spPriorScriptState),
        m_gilTaken(src.m_gilTaken),
        m_gstate(src.m_gstate)
    {
        src.m_pMarshaler = nullptr;
        src.m_spPriorScriptState = nullptr;
        src.m_gilTaken = false;
    }

private:

    ScriptSwitcher(const ScriptSwitcher&) =delete;
    ScriptSwitcher& operator=(const ScriptSwitcher&) =delete;

    Marshal::PythonMarshaler *m_pMarshaler;
    Microsoft::WRL::ComPtr<PythonScriptState> m_spPriorScriptState;

    bool m_gilTaken;
    PyGILState_STATE m_gstate;

};

// PinnedReference:
//
// Represents a pin (reference count) on a Python object.
//
class PinnedReference
{
public:

    PinnedReference() : m_pObject(nullptr)
    {
    }

    PinnedReference(_In_ const PinnedReference& from) : m_pObject(nullptr)
    {
        InternalAssign(from);
    }

    PinnedReference(_In_ PinnedReference&& from) : m_pObject(nullptr)
    {
        InternalMove(std::move(from));
    }

    PinnedReference(_In_ PyObject *pObject) : m_pObject(nullptr)
    {
        InternalAssign(pObject);
    }

    ~PinnedReference()
    {
        InternalClear();
    }

    PinnedReference& operator=(_In_ const PinnedReference& rhs)
    {
        InternalAssign(rhs);
        return *this;
    }

    PinnedReference& operator=(_In_ PinnedReference&& rhs)
    {
        InternalMove(std::move(rhs));
        return *this;
    }

    PinnedReference& operator=(_In_ PyObject *pObject)
    {
        InternalAssign(pObject);
        return *this;
    }

    operator PyObject *() const
    {
        return m_pObject;
    }

    bool operator==(_In_ const PinnedReference& rhs) const
    {
        return (m_pObject == rhs.m_pObject);
    }

    bool operator!=(_In_ const PinnedReference& rhs) const
    {
        return !(operator==(rhs));
    }

    bool operator==(_In_ const PyObject *pObject) const
    {
        return (m_pObject == pObject);
    }

    bool operator!=(_In_ const PyObject *pObject) const
    {
        return !(operator==(pObject));
    }

    PyObject *Detach()
    {
        PyObject *pObject = m_pObject;
        m_pObject = nullptr;
        return pObject;
    }

    static PinnedReference Take(_In_ PyObject *pObject)
    {
        return PinnedReference(pObject, true);
    }

    static PinnedReference Copy(_In_ PyObject *pObject)
    {
        return PinnedReference(pObject, false);
    }

protected:

    void InternalAssign(_In_ const PinnedReference& rhs)
    {
        InternalAssign(rhs.m_pObject);
    }

    void InternalAssign(_In_ PyObject *pObject)
    {
        InternalClear();
        m_pObject = pObject;
        InternalAddRef();
    }

    void InternalTake(_In_ PyObject *pObject)
    {
        InternalClear();
        m_pObject = pObject;
    }

    void InternalMove(_In_ PinnedReference&& rhs)
    {
        InternalClear();
        m_pObject = rhs.m_pObject;
        rhs.m_pObject = nullptr;
    }

    void InternalAddRef()
    {
        if (m_pObject != nullptr)
        {
            Py_INCREF(m_pObject);
        }
    }

    void InternalRelease()
    {
        if (m_pObject != nullptr)
        {
            Py_DECREF(m_pObject);
        }
    }

    void InternalClear()
    {
        InternalRelease();
        m_pObject = nullptr;
    }

private:

    PinnedReference(_In_ PyObject *pObject, _In_ bool take) :
        m_pObject(nullptr)
    {
        if (take)
        {
            InternalTake(pObject);
        }
        else
        {
            InternalAssign(pObject);
        }
    }

    PyObject *m_pObject;

};

//************************************************* 
// String Conversion (UTF8 <-> UTF16) Helpers:
//
BSTR SysAllocStringFromUTF8(_In_z_ const char* pUTF8);
HRESULT GetUTF16(_In_ PCSTR pUTF8, _Out_ std::wstring* pUTF16);
HRESULT GetUTF16N(_In_ PCSTR pUTF8, _In_ size_t n, _Out_ std::wstring* pUTF16);
HRESULT GetUTF8(_In_ PCWSTR pUTF16, _Out_ std::string* pUTF8);

} // Debugger::DataModel::ScriptProvider::Python
