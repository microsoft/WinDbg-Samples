//**************************************************************************
//
// Utility.h
//
// Common helper functions.  These would normally be present in a higher level library
// above the COM ABI.
//
//**************************************************************************

#ifndef IfFailedReturn
#define IfFailedReturn(EXPR) do { hr = (EXPR); if (FAILED(hr)) { return hr; }} while(FALSE, FALSE)
#endif // IfFailedReturn

// CreateProperty():
//
// From an instance of an IModelPropertyAccessor, create an object representation of the accessor.
//
HRESULT CreateProperty(_In_ IModelPropertyAccessor *pProperty,
	_COM_Outptr_ IModelObject **ppPropertyObject);

// CreateString():
//
// From a string value, create an object representation of the string.
//
HRESULT CreateString(_In_ PCWSTR pwszString,
	_COM_Outptr_ IModelObject **ppString);

// CreateInt():
//
// From an int value, create an object representation of the int.
//
HRESULT CreateInt(_In_ int value,
	_COM_Outptr_ IModelObject **ppInt);

