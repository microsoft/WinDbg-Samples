//**************************************************************************
//
// Marshal.cpp
//
// Marshaling constructs between Python objects and data model objects.
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

namespace Debugger::DataModel::ScriptProvider::Python::Marshal
{

//*************************************************
// Marshaling Objects Out Of Python:
//

HRESULT PythonSourceObject::RuntimeClassInitialize(_In_ PyObject *pPyObject,
                                                   _In_ PythonScriptState *pScriptState,
                                                   _In_ bool isDataModel,
                                                   _In_ bool isGlobalObject)
{
    HRESULT hr = S_OK;

    m_pythonObject = pPyObject;
    m_spOwningScriptState = pScriptState;
    m_isDataModel = isDataModel;
    m_isGlobalObject = isGlobalObject;

    return hr;
}

PythonSourceObject::~PythonSourceObject()
{
    //
    // We must be in the proper script context in order to release our references to Python!
    //
    auto switcher = m_spOwningScriptState->EnterScript();

    m_pythonObject = nullptr;
}

PythonMarshaler *PythonSourceObject::GetMarshaler() const
{
    return m_spOwningScriptState->GetScript()->GetMarshaler();
}

HRESULT PythonSourceObject::GetKey(_In_ IModelObject *pContextObject,
                                   _In_ PCWSTR pwszKey,
                                   _COM_Outptr_opt_result_maybenull_ IModelObject **ppKeyValue,
                                   _COM_Outptr_opt_result_maybenull_ IKeyStore **ppMetadata,
                                   _Out_opt_ bool *pHasKey)
{
    HRESULT hr = S_OK;

    // PUBLIC BOUNDARY CALL: Enter the appropriate script context
    auto switcher = m_spOwningScriptState->EnterScript();

    *pHasKey = false;
    if (ppKeyValue != nullptr)
    {
        *ppKeyValue = nullptr;
    }
    if (ppMetadata != nullptr)
    {
        *ppMetadata = nullptr;
    }

    std::string keyNameUtf8;
    IfFailedReturn(GetUTF8(pwszKey, &keyNameUtf8));

    //
    // Certain protocols (new protocols) are only marshal excluded if the script indicates that it supports
    // a particular verison of the API.
    //
    ULONG apiMajorVersion = 1;
    ULONG apiMinorVersion = 0;

    bool excludeFromMarshaling = ((IsGlobalObject() && GetMarshaler()->IsGlobalNameExcludedFromMarshaling(keyNameUtf8.c_str(),
                                                                                                          apiMajorVersion,
                                                                                                          apiMinorVersion)) ||
                                 (!IsGlobalObject() && GetMarshaler()->IsObjectNameExcludedFromMarshaling(keyNameUtf8.c_str(),
                                                                                                          apiMajorVersion,
                                                                                                          apiMinorVersion)));

    //
    // We should never get a request to fetch a key on an object for which we do not have the original
    // Python source object alive.
    //
    if (m_pythonObject == nullptr)
    {
        return E_INVALIDARG;
    }

    bool hasKey = !!PyObject_HasAttrString(m_pythonObject, keyNameUtf8.c_str());
    PyObject *pKeyValue = nullptr;

// @TODO
#if 0
    if (m_isDataModel)
    {
        //
        // If the object is a data model, the "this" pointer can be an object outside the context of Python.
        // In order to support that, we cannot simply PyObject_GetAttrString as we must know whether we are calling
        // a 'gatter' function.
        //
        // @TODO: This seems horribly expensive.  There should be a better way to get the outside "this" pointer
        //        injected into any accessors.
        //
        return E_NOTIMPL;
    }
    else
#endif // 0
    {
        if (hasKey && excludeFromMarshaling)
        {
            //
            // @TODO: Place the result in the side channel to allow the same script to pick it back up.
            //
            hasKey = false;
        }

        if (hasKey)
        {
            pKeyValue = PyObject_GetAttrString(m_pythonObject, keyNameUtf8.c_str());
            IfObjectErrorConvertAndReturn(pKeyValue);
        }
    }

    Object mshKeyValue;
    Metadata mshMetadata;
    if (hasKey && pKeyValue != nullptr)
    {
        IfFailedReturn(GetMarshaler()->MarshalFromPython(pKeyValue, &mshKeyValue, &mshMetadata));
    }

    *pHasKey = hasKey;
    if (ppKeyValue != nullptr)
    {
        *ppKeyValue = mshKeyValue.Detach();
    }
    if (ppMetadata != nullptr)
    {
        *ppMetadata = mshMetadata.Detach();
    }

    return hr;
}

HRESULT PythonSourceObject::EnumerateKeys(_In_ IModelObject *pContextObject,
                                          _COM_Outptr_ IKeyEnumerator **ppEnumerator)
{
    HRESULT hr = S_OK;
    *ppEnumerator = nullptr;

    // PUBLIC BOUNDARY CALL: Enter the appropriate script context
    auto switcher = m_spOwningScriptState->EnterScript();

    PyObject *pPyThis = m_pythonObject;
    if (m_isDataModel)
    {
        // @TODO: marshal pContextObject into Python
    }

    ComPtr<PythonKeyEnumerator> spEnum;
    IfFailedReturn(MakeAndInitialize<PythonKeyEnumerator>(&spEnum, this, pPyThis));
    *ppEnumerator = spEnum.Detach();
    return hr;
}

HRESULT PythonSourceObject::GetConcept(_In_ IModelObject * /*pContextObject*/,
                                       _In_ REFIID conceptId,
                                       _COM_Outptr_result_maybenull_ IUnknown **ppConceptInterface,
                                       _COM_Outptr_opt_result_maybenull_ IKeyStore **ppConceptMetadata,
                                       _Out_ bool *pHasConcept)
{
    HRESULT hr = S_OK;
    *ppConceptInterface = nullptr;
    if (ppConceptMetadata != nullptr)
    {
        *ppConceptMetadata = nullptr;
    }
    *pHasConcept = false;

    // PUBLIC BOUNDARY CALL: Enter the appropriate script context
    auto switcher = m_spOwningScriptState->EnterScript();

    if (conceptId == __uuidof(IDataModelConcept) && m_isDataModel)
    {
        ComPtr<IDataModelConcept> spDataModel = static_cast<IDataModelConcept *>(this);
        *ppConceptInterface = spDataModel.Detach();
        *pHasConcept = true;
    }
    else
    {
        //
        // Nothing right now.  It's an unknown concept.  We do not have support for it.
        //
        assert(!*pHasConcept);
    }

    return hr;
}

HRESULT PythonSourceObject::SetConcept(_In_ IModelObject * /*pContextObject*/,
                                       _In_ REFIID conceptId,
                                       _In_ IUnknown *pConceptInterface,
                                       _In_opt_ IKeyStore * /*pConceptMetadata*/)
{
    HRESULT hr = S_OK;

    // PUBLIC BOUNDARY CALL: Enter the appropriate script context
    auto switcher = m_spOwningScriptState->EnterScript();

    Marshal::PythonMarshaler *pMarshaler = GetMarshaler();

    //
    // We do not allow the arbitrary setting of another concept on a Python object.
    // There is no way to know how such translates at an arbitrary level to some Python
    // concept.
    //
    hr = E_FAIL;

    return hr;
}

HRESULT PythonSourceObject::NotifyParent(_In_ IModelObject * /*pParentModel*/)
{
    //
    // We are **EXPLICITLY** choosing to do nothing with the original parent notification.  Yes -- this will mean
    // that we do not immediately "see" LINQ and other such constructs on Python objects.  However, this prevents
    // two oddities:
    //
    // 1) We do not immediately have LINQ and other constructs appear on Python objects the first time they get
    //    marshaled into the data model.
    //
    // 2) We do not have to change the hierarchy of EVERY object that goes out into the data model.  This is extremely
    //    expensive.
    //
    return S_OK;
}

HRESULT PythonSourceObject::NotifyDestruct()
{
    //
    // Normally, when we marshal an object across the boundary, we keep a cache association to the marshaled object.
    // That cache association would get destroyed when this object (the PSO) is destroyed.  Normally, that would
    // happen when the marshaled object destructs.
    //
    // There is, however, one complication with this.  The PSO has interfaces for every concept it supports.  This means
    // it is possible for someone to have a concept interface still alive (which would keep the PSO alive) after the
    // marshaled object destructs.  It is illegal by spec to utilize that (since the pContext argument would have already
    // destructed); however -- we cannot keep a cache pointer into freed memory in the event someone destroyed interfaces
    // in an unfortunate order.
    //
    // The model will give us a notification when the object we associated the dynamic concept provider with goes away
    // and we will utilize this notification to remove our association with said object!
    //
#if 0
    m_attached.Data = nullptr;
    if (m_pCachePointer != nullptr)
    {
        m_pCachePointer->SetPointer(nullptr);
    }
#endif // 0
    return S_OK;
}

HRESULT PythonSourceObject::NotifyParentChange(_In_ IModelObject * /*pParentModel*/)
{
    HRESULT hr = S_OK;

    //
    // @TODO: Handle modifying the class hierarchy dynamically on the Python side...
    //

    return S_OK;
}

HRESULT PythonSourceObject::GetName(_Out_ BSTR *pModelName)
{
    *pModelName = nullptr;

    // PUBLIC BOUNDARY CALL: Enter the appropriate script context
    auto switcher = m_spOwningScriptState->EnterScript();

    // @TODO:
    return E_FAIL;
}

HRESULT PythonSourceObject::Call(_In_opt_ IModelObject *pContextObject,
                                 _In_ ULONG64 argCount,
                                 _In_reads_(argCount) IModelObject **ppArguments,
                                 _COM_Errorptr_ IModelObject **ppResult,
                                 _COM_Outptr_opt_result_maybenull_ IKeyStore **ppMetadata)
{
    HRESULT hr = S_OK;
    *ppResult = nullptr;
    if (ppMetadata != nullptr)
    {
        *ppMetadata = nullptr;
    }

    if (argCount > std::numeric_limits<Py_ssize_t>::max() - 1)
    {
        return E_INVALIDARG;
    }

    // PUBLIC BOUNDARY CALL: Enter the appropriate script context
    auto switcher = m_spOwningScriptState->EnterScript();

    PythonMarshaler *pMarshaler = GetMarshaler();

    // @TODO:
    bool isMethod = false;
    Py_ssize_t pythonArgCount = static_cast<Py_ssize_t>(isMethod ? argCount + 1 : argCount);

    PyObject *pTuple = PyTuple_New(pythonArgCount);
    IfObjectErrorConvertAndReturn(pTuple);
    auto tuple = PinnedReference::Take(pTuple);

    Py_ssize_t i = 0;
    if (isMethod)
    {
        //
        // @TODO: Marshal pContextObject...
        //
        if (PyTuple_SetItem(pTuple, i, Py_None) == -1)
        {
            return E_FAIL;
        }
        ++i;
    }

    while (i < pythonArgCount)
    {
        PyObject *pMarshaledArg = nullptr;
        IfFailedReturn(pMarshaler->MarshalToPython(nullptr, ppArguments[i - i], &pMarshaledArg));
        auto marshaledArg = PinnedReference::Take(pMarshaledArg);

        if (PyTuple_SetItem(pTuple, i, marshaledArg) == -1)
        {
            return E_FAIL;
        }

        //
        // NOTE: PyTuple_SetItem explicitly takes the reference count of the inpassed object.
        //       It **DOES NOT** increment it! 
        //
        marshaledArg.Detach();
        ++i;
    }

    Object mshResult;
    Metadata mshMetadata;

    PyObject *pResult = PyObject_Call(m_pythonObject, pTuple, nullptr);
    auto result = PinnedReference::Take(pResult);
    if (pResult == nullptr)
    {
        (void)GetMarshaler()->ConvertPythonException(E_FAIL, &mshResult, &hr);
        assert(FAILED(hr));
    }
    else
    {
        IfFailedReturn(GetMarshaler()->MarshalFromPython(pResult, &mshResult, &mshMetadata));
    }

    *ppResult = mshResult.Detach();
    if (ppMetadata != nullptr)
    {
        *ppMetadata = mshMetadata.Detach();
    }

    return hr;
}

PythonKeyEnumerator::~PythonKeyEnumerator()
{
    assert(m_spEnumSrc != nullptr);

    // We must be in the proper script context to release the Python objects held underneath us!
    auto switcher = m_spEnumSrc->GetScriptState()->EnterScript();
    m_pyThis = nullptr;
    m_pyEnumObj = nullptr;
}

HRESULT PythonKeyEnumerator::RuntimeClassInitialize(_In_ PythonSourceObject *pEnumSrc, _In_ PyObject *pPyThis)
{
    m_spEnumSrc = pEnumSrc;
    m_pyThis = pPyThis;
    m_objIsClass = (pPyThis != pEnumSrc->GetObject());
    return Reset();
}

HRESULT PythonKeyEnumerator::Reset()
{
    HRESULT hr = S_OK;

    // PUBLIC BOUNDARY CALL: Enter the appropriate script context
    auto switcher = m_spEnumSrc->GetScriptState()->EnterScript();

    m_pyEnumObj = m_spEnumSrc->GetObject();

    bool isBaseExternal = false;
#if 0
    if (m_objIsProto)
    {
        //
        // If we have an external object in the prototype chain, don't bounce the enumeration through
        // in and out of Python.  We lose information in translation.  Just pass it straight through.
        //
        DataModelSourceObject *pSourceObject;
        HRESULT hrOrigin = DataModelSourceObject::FromPythonObject(m_pyEnumObj, &pSourceObject);
        if (SUCCEEDED(hrOrigin))
        {
            isBaseExternal = true;
            IfFailedReturn(pSourceObject->GetObject()->EnumerateKeys(&m_spNativeEnum));
        }
    }
#endif // 0

    if (!isBaseExternal)
    {
        // m_spNativeEnum = nullptr;
        IfFailedReturn(FetchProperties());
        IfFailedReturn(FetchResolutionBases());
    }
    else
    {
        // @TODO:
        assert(false);
        return E_NOTIMPL;
    }

    return hr;
}

HRESULT PythonKeyEnumerator::GetNext(_Out_ BSTR *pKeyName,
                                     _COM_Errorptr_opt_ IModelObject **ppValue,
                                     _COM_Outptr_opt_result_maybenull_ IKeyStore **ppMetadata)
{
    HRESULT hr = S_OK;
    PythonMarshaler *pMarshaler = m_spEnumSrc->GetMarshaler();
    // Library::PythonLibrary *pLibrary = m_spEnumSrc->GetPythonLibrary();
    PythonScriptState *pActiveState = pMarshaler->GetActiveScriptState();

    *pKeyName = nullptr;
    if (ppValue != nullptr)
    {
        *ppValue = nullptr;
    }
    if (ppMetadata != nullptr)
    {
        *ppMetadata = nullptr;
    }

    // PUBLIC BOUNDARY CALL: Enter the appropriate script context
    auto switcher = m_spEnumSrc->GetScriptState()->EnterScript();

    Object nextObj;
    Metadata nextMetadata;

    char const *pszPropertyName = nullptr;

    for(;;)
    {
        if (m_pyEnumObj == nullptr)
        {
            return E_BOUNDS;
        }

        while(m_cur >= m_itemListCount)
        {
            IfFailedReturn(AdvanceMRO());
            if (m_pyEnumObj == nullptr)
            {
                return E_BOUNDS;
            }
        }

#if 0
        //
        // If one of the prototypes in the chain is a proxy for an external object, go straight to the source --
        // going through the proxy to get the information is worthless -- it incurs a marshaling and unmarshaling and
        // can potentially lose information (such as metadata)
        //
        if (m_spNativeEnum != nullptr)
        {
            HRESULT hrNativeEnum = m_spNativeEnum->GetNext(pKeyName, ppValue, ppMetadata);
            if (SUCCEEDED(hrNativeEnum))
            {
                return hrNativeEnum;
            }
            else
            {
                //
                // We hit the end of the iterator (or other failure on the native side).  Go to the next class.
                //
                m_cur = m_propNamesLength;
                continue;
            }
        }
#endif // 0

        //
        // The item is a {key, value} tuple
        //
        PyObject *pItem = PyList_GetItem(m_pyEnumObjDictItemList, m_cur);
        IfObjectErrorConvertAndReturn(pItem);

        PyObject *pKey = PyTuple_GetItem(pItem, 0);
        IfObjectErrorConvertAndReturn(pKey);

        PyObject *pValue = PyTuple_GetItem(pItem, 1);
        IfObjectErrorConvertAndReturn(pValue);

        ++m_cur;

        //
        // Remember that there are things which will appear in the properties list which are *NOT*
        // what we would consider or map to data model keys. 
        //
        // Filter any such constructs out!
        //

        pszPropertyName = PyUnicode_AsUTF8AndSize(pKey, nullptr);

        if ((m_spEnumSrc->IsGlobalObject() && pMarshaler->IsGlobalNameExcludedFromMarshaling(pszPropertyName)) ||
            (!m_spEnumSrc->IsGlobalObject() && pMarshaler->IsObjectNameExcludedFromMarshaling(pszPropertyName)))
        {
            continue;
        }

        //
        // The fetch may be to a getter on a prototype attached to native code, so we must manually
        // invoke the getter.  Likewise, such code may throw an exception and we need to be prepared
        // to translate that back to the data model.
        //
        if (ppValue != nullptr)
        {
            //
            // @TODO: Deal with this...
            //
            {
                //
                // Metadata on a property is associated with the key and must be fetchable without fetching
                // the value.  A valueWithMetadata(...) is ignored in favor of any metadata on the key.
                //
                IfFailedReturn(pMarshaler->MarshalFromPython(pValue, &nextObj, nullptr));
            }
        }

        if (ppMetadata != nullptr)
        {
            //
            // @TODO: Hook up metadata...
            //
        }

        break;
    }

    *pKeyName = SysAllocStringFromUTF8(pszPropertyName);
    if (*pKeyName == nullptr)
    {
        return E_OUTOFMEMORY;
    }
    if (ppValue != nullptr)
    {
        *ppValue = nextObj.Detach();
    }
    if (ppMetadata != nullptr)
    {
        *ppMetadata = nextMetadata.Detach();
    }

    return hr;
}

HRESULT PythonKeyEnumerator::FetchProperties()
{
    HRESULT hr = S_OK;

    m_pyEnumObjDict = PyObject_GenericGetDict(m_pyEnumObj, nullptr);
    IfObjectErrorConvertAndReturn(m_pyEnumObjDict);

    m_pyEnumObjDictItemList = PyDict_Items(m_pyEnumObjDict);
    IfObjectErrorConvertAndReturn(m_pyEnumObjDictItemList);

    m_itemListCount = PyList_Size(m_pyEnumObjDictItemList);
    m_cur = 0;

    return hr;
}

HRESULT PythonKeyEnumerator::FetchResolutionBases()
{
    HRESULT hr = S_OK;

    m_pyMro = nullptr;
    m_mroCount = 0;
    m_mroCur = 0;

    PyObject *pTypeObject = PyObject_Type(m_spEnumSrc->GetObject());
    IfObjectErrorConvertAndReturn(pTypeObject);
    auto typeObject = PinnedReference::Take(pTypeObject);

    if (PyObject_HasAttrString(pTypeObject, "__mro__"))
    {
        m_pyMro = PyObject_GetAttrString(pTypeObject, "__mro__");
        IfObjectErrorConvertAndReturn(m_pyMro);
        m_mroCount = PyTuple_Size(m_pyMro);
    }

    return hr;
}

HRESULT PythonKeyEnumerator::AdvanceMRO()
{
    HRESULT hr = S_OK;
    bool hasProto = false;

    if (m_mroCur >= m_mroCount)
    {
        m_pyEnumObj = nullptr;
        m_pyEnumObjDict = nullptr;
        m_pyEnumObjDictItemList = nullptr;
        m_itemListCount = 0;
        m_cur = 0;
        return S_FALSE;
    }

    m_pyEnumObj = PyTuple_GetItem(m_pyMro, m_mroCur);
    IfObjectErrorConvertAndReturn(m_pyEnumObj);

    ++m_mroCur;

    bool isBaseExternal = false;

    if (!isBaseExternal)
    {
        IfFailedReturn(FetchProperties());
    }
    else
    {
        // @TODO:
        assert(false);
        return E_NOTIMPL;
    }

    return hr;
}

//*************************************************
// Marshaling Objects Into Python:
//

HRESULT PythonMarshaler::MarshalToPython(_In_opt_ IModelObject *pSrcObject,
                                         _In_ IModelObject *pModelObject,
                                         _Out_ PyObject **ppPyObject)
{
    HRESULT hr = S_OK;
    *ppPyObject = nullptr;

    PyObject *pPyObject = nullptr;

    ModelObjectKind mk;
    IfFailedReturn(pModelObject->GetKind(&mk));
    switch(mk)
    {
        // ObjectIntrinsic:
        //
        // Comes out as varying types:
        //
        // I1-I8    --> Python long
        // UI1-UI8  --> Python long
        // (String) --> Python Unicode
        // bool     --> Python bool
        //
        // pointer  --> library type (not yet complete)
        //
        case ObjectIntrinsic:
        {
            //
            // Pointers are intrinsic VT_UI8 values with additional (pointer) type information.  Do *NOT* marshal
            // such objects in by value.  They become library objects.  We need to preserve the type information
            // and project additional APIs on the pointer object.
            //
            ComPtr<IDebugHostType> spType;
            TypeKind tk;
            if (SUCCEEDED(pModelObject->GetTypeInfo(&spType)) &&
                spType != nullptr &&
                SUCCEEDED(spType->GetTypeKind(&tk)) &&
                tk == TypePointer)
            {
                //
                // @TODO: Marshal pointers with a special library type.
                //
            }

            pPyObject = ModelValueToPython(pModelObject);

            if (pPyObject == nullptr)
            {
                return E_INVALIDARG;
            }

            break;
        }

        //
        // @TODO: A whole bunch of other objects:
        //
        //        ObjectNoValue
        //        ObjectError
        //        ObjectMethod
        //        ObjectContext
        //        ObjectSynthetic
        //        ObjectTargetObject
        //        ObjectTargetObjectReference
        //        ObjectKeyReference
        //

        default:
            //
            // @TODO: Other types.
            //
            return E_NOTIMPL;
    }

    *ppPyObject = pPyObject;
    return hr;
}

PyObject *PythonMarshaler::ModelValueToPython(_In_ IModelObject * pModelObject)
{
    PyObject *pPyObject = nullptr;

    VARIANT vtVal;
    if (FAILED(pModelObject->GetIntrinsicValue(&vtVal)))
    {
        return nullptr;
    }
    variant_ptr spvVal(&vtVal);

    switch (vtVal.vt)
    {
    case VT_I1:
        pPyObject = PyLong_FromLong(static_cast<long>(vtVal.cVal));
        break;

    case VT_I2:
        pPyObject = PyLong_FromLong(static_cast<long>(vtVal.iVal));
        break;

    case VT_I4:
        pPyObject = PyLong_FromLong(static_cast<long>(vtVal.lVal));
        break;

    case VT_I8:
        pPyObject = PyLong_FromLongLong(static_cast<long long>(vtVal.llVal));
        break;

    case VT_UI1:
        pPyObject = PyLong_FromUnsignedLong(static_cast<unsigned long>(vtVal.bVal));
        break;

    case VT_UI2:
        pPyObject = PyLong_FromUnsignedLong(static_cast<unsigned long>(vtVal.uiVal));
        break;

    case VT_UI4:
        pPyObject = PyLong_FromUnsignedLong(static_cast<unsigned long>(vtVal.ulVal));
        break;

    case VT_UI8:
        pPyObject = PyLong_FromUnsignedLongLong(static_cast<unsigned long long>(vtVal.ullVal));
        break;

    case VT_R4:
        pPyObject = PyFloat_FromDouble(static_cast<double>(vtVal.fltVal));
        break;

    case VT_R8:
        pPyObject = PyFloat_FromDouble(static_cast<double>(vtVal.dblVal));
        break;

    case VT_BOOL:
        pPyObject = PyBool_FromLong(static_cast<long>(vtVal.boolVal != VARIANT_FALSE));
        break;

    case VT_BSTR:
        pPyObject = PyUnicode_FromWideChar(static_cast<const wchar_t *>(vtVal.bstrVal), -1);
        break;

    default:
        VariantClear(&vtVal);

    }

    return pPyObject;
}

//*************************************************
// General Marshaling:
//

HRESULT PythonMarshaler::Initialize()
{
    HRESULT hr = S_OK;

    return hr;
}

HRESULT PythonMarshaler::MarshalFromPython(_In_ PyObject *pPyObject,
                                           _Out_ Object *pModelObject,
                                           _Out_opt_ Metadata *pObjectMetadata,
                                           _In_ bool isDataModel,
                                           _In_ bool isGlobalObject)
{
    HRESULT hr = S_OK;
    *pModelObject = Object();
    if (pObjectMetadata != nullptr)
    {
        *pObjectMetadata = Metadata();
    }

    Object mshResult;
    Metadata mshMetadata;

    auto fn = [&]()
    {
        if (PyUnicode_Check(pPyObject))
        {
            const char *pStr = PyUnicode_AsUTF8AndSize(pPyObject, nullptr);

            //
            // There is no direct boxing implementation for narrow strings (particularly in the UTF8 format).
            // Convert to UTF16-LE before boxing it back.  Yes -- this is a required double alloc.
            //
            std::wstring strUtf16;
            IfFailedReturn(GetUTF16(pStr, &strUtf16));
            mshResult = strUtf16;
        }
        else if (PyLong_Check(pPyObject))
        {
            //
            // PyLong is an arbitrary precision integer.  We have no representation of such within the data
            // model.  Attempt to take it out as a long long (64-bit).  If it overflows in the positive direction,
            // take it out as unsigned long long.
            //
            // As with JS, take it out into the data model as unsigned 64-bit if >=0 and signed if <0.  If it cannot
            // fit within the precision of a data model 64-bit, throw.
            //
            int ovf;
            long long ll = PyLong_AsLongLongAndOverflow(pPyObject, &ovf);
            if (ovf == 1)
            {
                unsigned long long ull = PyLong_AsUnsignedLongLong(pPyObject);
                mshResult = ull;
            }
            else if (ovf != 0 || PyErr_Occurred())
            {
                return E_FAIL;
            }

            if (ll >= 0)
            {
                mshResult = static_cast<unsigned long long>(ll);
            }
            else
            {
                mshResult = ll;
            }
        }
        else if (PyCallable_Check(pPyObject))
        {
            ComPtr<PythonSourceObject> spSrcObj;
            IModelMethod *pMethod;

            IfFailedReturn(MakeAndInitialize<PythonSourceObject>(&spSrcObj, pPyObject, m_spActiveScriptState.Get(), false, false));
            pMethod = static_cast<IModelMethod *>(spSrcObj.Get());

            ComPtr<IModelObject> spMethodObject;
            VARIANT vtUnk;
            vtUnk.vt = VT_UNKNOWN;
            vtUnk.punkVal = pMethod;
            IfFailedReturn(GetManager()->CreateIntrinsicObject(ObjectMethod, &vtUnk, &spMethodObject));

            mshResult = std::move(spMethodObject);
        }
        else
        {
            //
            // @TODO: A **LOT** more needs to happen here.
            //

            //
            // Pin the Python object and create a synthetic to represent this on the data model side
            // with a set of dynamic providers linked back to Python.
            //
            // The model side of this should be a "shadow" which redirects everything into Python.
            //
            ComPtr<PythonSourceObject> spSrcObj;
            IfFailedReturn(MakeAndInitialize<PythonSourceObject>(&spSrcObj, pPyObject, m_spActiveScriptState.Get(), isDataModel, isGlobalObject));

            mshResult = Object::Create(HostContext());
            IfFailedReturn(mshResult->SetConcept(__uuidof(IDynamicKeyProviderConcept), static_cast<IDynamicKeyProviderConcept *>(spSrcObj.Get()), nullptr));
            IfFailedReturn(mshResult->SetConcept(__uuidof(IDynamicConceptProviderConcept), static_cast<IDynamicConceptProviderConcept *>(spSrcObj.Get()), nullptr));
        }

        return S_OK;
    };
    IfFailedReturn(ConvertException(fn));

    *pModelObject = std::move(mshResult);
    if (pObjectMetadata != nullptr)
    {
        *pObjectMetadata = std::move(mshMetadata);
    }

    return hr;
}

HRESULT PythonMarshaler::ConvertPythonException(_In_ HRESULT hrConverted,
                                                _Out_ Object *pErrorObject,
                                                _Out_ HRESULT *pHrFinal)
{
    HRESULT hr = S_OK;
    *pErrorObject = Object();
    *pHrFinal = hrConverted;

    auto fn = [&]()
    {
        PythonScriptState *pActiveState = GetActiveScriptState();
        PythonScript *pScript = pActiveState->GetScript();

        bool hasException = PyErr_Occurred();

        assert(hasException);
        if (!hasException)
        {
            return hr;
        }

        PyObject *pType, *pValue, *pTraceback;
        PyErr_Fetch(&pType, &pValue, &pTraceback);

        auto type = PinnedReference::Take(pType);
        auto value = PinnedReference::Take(pValue);
        auto traceBack = PinnedReference::Take(pTraceback);

        const char *pErrStr = nullptr;
        std::wstring errStr;
        if (pValue != nullptr)
        {
            PyObject *pStr = PyObject_Str(value);
            auto str = PinnedReference::Take(pStr);
            pErrStr = PyUnicode_AsUTF8AndSize(str, nullptr);
            IfFailedReturn(GetUTF16(pErrStr, &errStr));
        }

        *pErrorObject = Object::CreateError(hr, errStr.c_str());
        return hr;
    };
    return ConvertException(fn);
}

HRESULT PythonMarshaler::SetDataModelError(_In_ HRESULT hrFail,
                                           _In_opt_ IModelObject *pErrorObject)
{
    HRESULT hr = S_OK;

    bool hasException = (PyErr_Occurred() != nullptr);

    if (!hasException)
    {
        // @TODO:
        PyErr_SetString(PyExc_RuntimeError, "oopsie: we need to flesh this out...");
    }

    return hr;
}

HRESULT PythonMarshaler::SetActiveScriptState(_In_opt_ PythonScriptState *pScriptState, 
                                              _In_ ScriptEntryType entryType)
{
    HRESULT hr = S_OK;

    PythonScript *pEntryExitScript = nullptr;
    if (entryType == ScriptEntryType::ScriptEntry && pScriptState != nullptr)
    {   
        pEntryExitScript = pScriptState->GetScript();
//        pEntryExitScript->MarkMonitored();
    }
    else if (entryType == ScriptEntryType::ScriptExit && m_spActiveScriptState != nullptr)
    {
        pEntryExitScript = m_spActiveScriptState->GetScript();
//        pEntryExitScript->ClearMonitored();
    }

    // 
    // @TODO: debugger: break on abort...
	//

    if (pScriptState == nullptr)
    {
        //
        // @TODO: separate interpreters...
        //
        m_spActiveScriptState = pScriptState;
    }
    else
    {
        //
        // @TODO: separate interpreters...
        //
        m_spActiveScriptState = pScriptState;
    }

    //
    // @TODO: monitor thread...
    //

    return hr;
}

} // Debugger::DataModel::ScriptProvider::Python::Marshal


