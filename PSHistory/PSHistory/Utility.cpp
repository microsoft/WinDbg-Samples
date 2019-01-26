//**************************************************************************
//
// Utility.cpp
//
// Common helper functions.  These would normally be present in a higher level library
// above the COM ABI.
//
//**************************************************************************

#include "SimpleIntro.h"

using namespace Microsoft::WRL;

// CreateProperty():
//
// From an instance of an IModelPropertyAccessor, create an object representation of the accessor.
// 
HRESULT CreateProperty(_In_ IModelPropertyAccessor *pProperty,
    _COM_Outptr_ IModelObject **ppPropertyObject)
{
    HRESULT hr = S_OK;
    *ppPropertyObject = nullptr;

    VARIANT vtVal;
    vtVal.vt = VT_UNKNOWN;
    vtVal.punkVal = pProperty;

    ComPtr<IModelObject> spPropertyObject;
    IfFailedReturn(GetManager()->CreateIntrinsicObject(ObjectPropertyAccessor, &vtVal, &spPropertyObject));

    *ppPropertyObject = spPropertyObject.Detach();
    return hr;
}

// CreateString():
//
// From a string value, create an object representation of the string.
//
HRESULT CreateString(_In_ PCWSTR pwszString,
    _COM_Outptr_ IModelObject **ppString)
{
    HRESULT hr = S_OK;
    *ppString = nullptr;

    VARIANT vtVal;
    vtVal.vt = VT_BSTR;
    vtVal.bstrVal = SysAllocString(pwszString);
    if (vtVal.bstrVal == nullptr)
    {
        return E_OUTOFMEMORY;
    }

    ComPtr<IModelObject> spValue;
    hr = GetManager()->CreateIntrinsicObject(ObjectIntrinsic, &vtVal, &spValue);
    SysFreeString(vtVal.bstrVal);
    IfFailedReturn(hr);

    *ppString = spValue.Detach();
    return hr;
}

// CreateInt():
//
// From an int value, create an object representation of the int.
//
HRESULT CreateInt(_In_ int value,
    _COM_Outptr_ IModelObject **ppInt)
{
    HRESULT hr = S_OK;
    *ppInt = nullptr;

    VARIANT vtVal;
    vtVal.vt = VT_I4;
    vtVal.lVal = value;

    ComPtr<IModelObject> spValue;
    IfFailedReturn(GetManager()->CreateIntrinsicObject(ObjectIntrinsic, &vtVal, &spValue));

    *ppInt = spValue.Detach();
    return hr;
}
