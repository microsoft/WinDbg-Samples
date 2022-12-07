//**************************************************************************
//
// PyClasses.cpp
//
// General support (and implementations of) Python classes
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#include "PyProvider.h"

namespace Debugger::DataModel::ScriptProvider::Python::Classes
{

//**************************************************************************
// General Infrastructure:
//

PyType_Slot PythonClass::s_classSlots[] =
{
    { 0, nullptr },                                     // Slot zero is a terminator
    { 1, nullptr },                                     // Py_bf_getbuffer 1
    { 2, nullptr },                                     // Py_bf_releasebuffer 2
    { 3, nullptr },                                     // Py_mp_ass_subscript 3
    { 4, nullptr },                                     // Py_mp_length 4
    { 5, nullptr },                                     // Py_mp_subscript 5
    { 6, nullptr },                                     // Py_nb_absolute 6
    { 7, nullptr },                                     // Py_nb_add 7
    { 8, nullptr },                                     // Py_nb_and 8
    { 9, nullptr },                                     // Py_nb_bool 9
    { 10, nullptr },                                    // Py_nb_divmod 10
    { 11, nullptr },                                    // Py_nb_float 11
    { 12, nullptr },                                    // Py_nb_floor_divide 12
    { 13, nullptr },                                    // Py_nb_index 13
    { 14, nullptr },                                    // Py_nb_inplace_add 14
    { 15, nullptr },                                    // Py_nb_inplace_and 15
    { 16, nullptr },                                    // Py_nb_inplace_floor_divide 16
    { 17, nullptr },                                    // Py_nb_inplace_lshift 17
    { 18, nullptr },                                    // Py_nb_inplace_multiply 18
    { 19, nullptr },                                    // Py_nb_inplace_or 19
    { 20, nullptr },                                    // Py_nb_inplace_power 20
    { 21, nullptr },                                    // Py_nb_inplace_remainder 21
    { 22, nullptr },                                    // Py_nb_inplace_rshift 22
    { 23, nullptr },                                    // Py_nb_inplace_subtract 23
    { 24, nullptr },                                    // Py_nb_inplace_true_divide 24
    { 25, nullptr },                                    // Py_nb_inplace_xor 25
    { 26, nullptr },                                    // Py_nb_int 26
    { 27, nullptr },                                    // Py_nb_invert 27
    { 28, nullptr },                                    // Py_nb_lshift 28
    { 29, nullptr },                                    // Py_nb_multiply 29
    { 30, nullptr },                                    // Py_nb_negative 30
    { 31, nullptr },                                    // Py_nb_or 31
    { 32, nullptr },                                    // Py_nb_positive 32
    { 33, nullptr },                                    // Py_nb_power 33
    { 34, nullptr },                                    // Py_nb_remainder 34
    { 35, nullptr },                                    // Py_nb_rshift 35
    { 36, nullptr },                                    // Py_nb_subtract 36
    { 37, nullptr },                                    // Py_nb_true_divide 37
    { 38, nullptr },                                    // Py_nb_xor 38
    { 39, nullptr },                                    // Py_sq_ass_item 39
    { 40, nullptr },                                    // Py_sq_concat 40
    { 41, nullptr },                                    // Py_sq_contains 41
    { 42, nullptr },                                    // Py_sq_inplace_concat 42
    { 43, nullptr },                                    // Py_sq_inplace_repeat 43
    { 44, nullptr },                                    // Py_sq_item 44
    { 45, nullptr },                                    // Py_sq_length 45
    { 46, nullptr },                                    // Py_sq_repeat 46
    { 47, &PythonClass::Bridge_TpAlloc },               // Py_tp_alloc 47
    { 48, nullptr },                                    // Py_tp_base 48
    { 49, nullptr },                                    // Py_tp_bases 49
    { 50, nullptr },                                    // Py_tp_call 50
    { 51, nullptr },                                    // Py_tp_clear 51
    { 52, &PythonClass::Bridge_TpDealloc },             // Py_tp_dealloc 52
    { 53, nullptr },                                    // Py_tp_del 53
    { 54, nullptr },                                    // Py_tp_descr_get 54
    { 55, nullptr },                                    // Py_tp_descr_set 55
    { 56, nullptr },                                    // Py_tp_doc 56
    { 57, nullptr },                                    // Py_tp_getattr 57
    { 58, nullptr },                                    // Py_tp_getattro 58
    { 59, nullptr },                                    // Py_tp_hash 59
    { 60, &PythonClass::Bridge_TpInit },                // Py_tp_init 60
    { 61, nullptr },                                    // Py_tp_is_gc 61
    { 62, nullptr },                                    // Py_tp_iter 62
    { 63, nullptr },                                    // Py_tp_iternext 63
    { 64, nullptr },                                    // Py_tp_methods 64
    { 65, nullptr },                                    // Py_tp_new 65
    { 66, nullptr },                                    // Py_tp_repr 66
    { 67, nullptr },                                    // Py_tp_richcompare 67
    { 68, nullptr },                                    // Py_tp_setattr 68
    { 69, nullptr },                                    // Py_tp_setattro 69
    { 70, nullptr },                                    // Py_tp_str 70
    { 71, &PythonClass::Bridge_TpTraverse },            // Py_tp_traverse 71
    { 72, nullptr },                                    // Py_tp_members 72
    { 73, nullptr },                                    // Py_tp_getset 73
    { 74, nullptr },                                    // Py_tp_free 74
    { 75, nullptr },                                    // Py_nb_matrix_multiply 75
    { 76, nullptr },                                    // Py_nb_inplace_matrix_multiply 76
    { 77, nullptr },                                    // Py_am_await 77
    { 78, nullptr },                                    // Py_am_aiter 78
    { 79, nullptr },                                    // Py_am_anext 79
    { 80, nullptr },                                    // Py_tp_finalize 80
    { 81, nullptr }                                     // Py_am_send 81
};

std::unordered_map<ULONG_PTR, PythonClass *> PythonClass::s_classMap;

PythonClass::PythonClass(_In_ char const *pName, 
                         _In_ unsigned int pyFlags,
                         _In_ int instanceDataSize, 
                         _In_ int itemDataSize) :
    m_pMarshaler(nullptr),          // comes in later
    m_pClass(nullptr)               // dynamically created later
{
    m_classSpec.name = pName;
    m_classSpec.basicsize = sizeof(Data) + instanceDataSize;
    m_classSpec.itemsize = itemDataSize;
    m_classSpec.flags = pyFlags;
}

PythonClass::~PythonClass()
{
    //
    // With this class going away, make sure the global entry which maps its type object to this instance
    // goes away along with it.
    //
    if (m_pClass)
    {
        s_classMap.erase(reinterpret_cast<ULONG_PTR>(m_pClass));
    }
}

PythonClass *PythonClass::FromTypeObject(_In_ PyTypeObject *pClass)
{
    auto it = s_classMap.find(reinterpret_cast<ULONG_PTR>(pClass));
    if (it == s_classMap.end()) { return nullptr; }
    return it->second;
}

PyObject* PythonClass::TpAlloc(_In_ PyTypeObject *pTypeObject, _In_ Py_ssize_t nItems)
{
    //
    // Call the general Python allocator and **ENSURE** that we fill in the 'Class' field of the header
    // so that we can do generic dispatch.
    //
    PyObject *pObject = PyType_GenericAlloc(pTypeObject, nItems);
    if (pObject == nullptr) { return nullptr; }
    Data *pObjectData = reinterpret_cast<Data *>(pObject);
    pObjectData->Class = this;
    return pObject;
}

HRESULT PythonClass::AddSlot(_In_ int slot)
{
    //
    // We do not have every slot number hooked up to appropriate virtual methods.  If this is valid,
    // add it to our actual slot definition.
    //
    if (slot == 0 || slot >= ARRAYSIZE(s_classSlots)) { return E_INVALIDARG; }
    PyType_Slot *pSlotData = &s_classSlots[slot];
    if (pSlotData->pfunc == nullptr) { return E_INVALIDARG; }
    return ConvertException([&]() { m_slots.push_back(*pSlotData); return S_OK;  });
}

HRESULT PythonClass::Initialize(_In_ Marshal::PythonMarshaler *pMarshaler)
{
    HRESULT hr = S_OK;

    //
    // We **MUST** have tp_alloc in order to get into the allocation path to hookup the "Data->Class" pointer
    // for dynamic dispatch.  If the derived class hasn't implemented this slot, we do here.  If they did, they
    // have a responsibility to call us or otherwise honor that protocol.
    //
    if (!HasDefinedSlot(Py_tp_alloc))
    {
        IfFailedReturn(AddSlot(Py_tp_alloc));
    }

    IfFailedReturn(ConvertException([&]() { m_slots.push_back({ 0, nullptr }); return S_OK; }));
    m_classSpec.slots = &(m_slots[0]);

    assert(((m_classSpec.flags & Py_TPFLAGS_MANAGED_DICT) != 0) == ((m_classSpec.flags & Py_TPFLAGS_HAVE_GC) != 0));
    assert((m_classSpec.flags & Py_TPFLAGS_HAVE_GC) == 0 || HasDefinedSlot(Py_tp_traverse));

    m_pClass = PyType_FromSpec(&m_classSpec);
    IfObjectErrorConvertAndReturn(m_pClass);

    //
    // NOTE: There is no "context pointer" to stuff a relationship from the type object back to the PythonClass
    //       instance.  Instance objects have data which can point back.  The type object itself does not.  It's
    //       generally expected that the context is carried by whatever functions are on the class itself.  As
    //       we are an abstraction, this is somewhat difficult.  Generally, common types are unique global PyType_*
    //       objects.  We keep a "global map" of our type objects (class types) to PythonClasses to have this context
    //
    IfFailedReturn(ConvertException([&](){
        s_classMap.insert( { reinterpret_cast<ULONG_PTR>(m_pClass), this});
        return S_OK;
    }));

    return hr;
}

HRESULT PythonClass::AddToObject(_In_ PyObject *pObject)
{
    //
    // If we are explicitly being asked to add this to a dictionary, it's not an attribute...
    //
    int result = 0;
    if (PyDict_Check(pObject))
    {
        result = PyDict_SetItemString(pObject, GetName(), GetClassObject());
    }
    else
    {
        result = PyObject_SetAttrString(pObject, GetName(), GetClassObject());
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

PyObject *PythonClass::CreateInstance(_In_ PyObject *pArgs, _In_ PyObject *pKwArgs)
{
    PinnedReference argsTuple;
    if (pArgs == nullptr)
    {
        argsTuple = PinnedReference::Take(PyTuple_New(0));
        if (argsTuple == nullptr)
        {
            return nullptr;
        }
        pArgs = argsTuple;
    }

    // The Python docs ABSOLUTELY SUCK.  The *Class/Instance*_New are gone in Python 3...  without clear guidance...
    // StackOverflow suggests just calling the class...  Doing so crashes deep in the guts of Python with
    // Py_TPFLAGS_MANAGED_DICT touching an invalid pointer as if the object wasn't allocated correctly. 

    return PyObject_Call(GetClassObject(), pArgs, pKwArgs);
}

//**************************************************************************
// Specific Implementation:
//

HRESULT PythonNamespace::Initialize(_In_ Marshal::PythonMarshaler *pMarshaler)
{
    HRESULT hr = S_OK;
    IfFailedReturn(AddSlots(1, Py_tp_traverse));
    return PythonClass::Initialize(pMarshaler);
}

HRESULT PythonTypeSignatureRegistration::Initialize(_In_ Marshal::PythonMarshaler *pMarshaler)
{
    HRESULT hr = S_OK;
    IfFailedReturn(AddSlots(2, Py_tp_init, Py_tp_traverse));
    return PythonClass::Initialize(pMarshaler);
}

int PythonTypeSignatureRegistration::TpInit(_In_ PyObject *pSelf, _In_ PyObject *pArgs, _In_ PyObject *pKwArgs)
{
    Py_ssize_t argCount = PyTuple_Size(pArgs);
    if (argCount != 2)
    {
        PyErr_Format(PyExc_Exception, "invalid argument");
        return -1;
    }

    int i = PyObject_SetAttrString(pSelf, "class_object", PyTuple_GetItem(pArgs, 0));
    int j = PyObject_SetAttrString(pSelf, "signature_string", PyTuple_GetItem(pArgs, 1));

    return 0;
}

} // Debugger::DataModel::ScriptProvider::Python::Classes
