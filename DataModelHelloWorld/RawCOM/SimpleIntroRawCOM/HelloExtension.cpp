//**************************************************************************
//
// HelloExtension.cpp
//
// Properties and data models which extend the debugger's notion of what a process
// is to include new properties.
//
//**************************************************************************

#include "SimpleIntro.h"

using namespace Microsoft::WRL;

HRESULT HelloData::RuntimeClassInitialize(_In_ PCWSTR pwszText)
{
	//
	// C++ exceptions must be stopped at the COM boundary.
	//
	HRESULT hr = S_OK;
	try
	{
		m_Text = pwszText;
	}
	catch (std::exception const&)
	{
		hr = E_OUTOFMEMORY;
	}
	return hr;
}

HRESULT TestProperty::GetValue(_In_ PCWSTR /*pwszKey*/,                         // Always "Test" for this
	_In_opt_ IModelObject * /*pHelloInstance*/,      // The instance of our 'hello' type
	_COM_Outptr_ IModelObject **ppValue)
{
	HRESULT hr = S_OK;
	*ppValue = nullptr;

	//
	// Create an object with two keys: 'A' with the integer value 42 and 'B' with the string value "Hello World"
	//
	ComPtr<IModelObject> spA;
	IfFailedReturn(CreateInt(42, &spA));

	ComPtr<IModelObject> spB;
	IfFailedReturn(CreateString(L"Hello World", &spB));

	ComPtr<IModelObject> spObject;
	IfFailedReturn(GetManager()->CreateSyntheticObject(nullptr, &spObject));
	IfFailedReturn(spObject->SetKey(L"A", spA.Get(), nullptr));
	IfFailedReturn(spObject->SetKey(L"B", spB.Get(), nullptr));

	*ppValue = spObject.Detach();
	return hr;
}

HRESULT WorldProperty::GetValue(_In_ PCWSTR /*pwszKey*/,                        // Always "World" for this
	_In_opt_ IModelObject *pHelloInstance,          // The instance of our 'hello' type
	_COM_Outptr_ IModelObject **ppValue)
{
	HRESULT hr = S_OK;
	*ppValue = nullptr;

	//
	// Fetch the instance data that was associated with this type when we created it.
	//
	ComPtr<IUnknown> spHelloDataUnknown;
	IfFailedReturn(pHelloInstance->GetContextForDataModel(m_pDataModel, &spHelloDataUnknown));
	HelloData *pHelloData = static_cast<HelloData *>(spHelloDataUnknown.Get());

	//
	// Box the string that we want to return.  This means we need to create a variant BSTR for it.
	//
	return CreateString(pHelloData->m_Text.c_str(), ppValue);
}

// HelloStringConversion::ToDisplayString():
//
// Converts an instance of the "Hello" object to a display string.
//
HRESULT HelloStringConversion::ToDisplayString(_In_ IModelObject *pHelloInstance,
	_In_opt_ IKeyStore * /*pMetadata*/,
	_Out_ BSTR *pDisplayString)
{
	HRESULT hr = S_OK;
	*pDisplayString = nullptr;

	//
	// Fetch the instance data that was associated with this type when we created it.
	//
	ComPtr<IUnknown> spHelloDataUnknown;
	IfFailedReturn(pHelloInstance->GetContextForDataModel(m_pDataModel, &spHelloDataUnknown));
	HelloData *pHelloData = static_cast<HelloData *>(spHelloDataUnknown.Get());

	//
	// Create the string conversion (bounding exceptions at the COM boundary)
	//
	std::wstring stringConversion;
	try
	{
		stringConversion = L"COM Object: ";
		stringConversion += pHelloData->m_Text;
	}
	catch (std::exception const&)
	{
		hr = E_OUTOFMEMORY;
	}

	if (SUCCEEDED(hr))
	{
		*pDisplayString = SysAllocString(stringConversion.c_str());
		hr = (*pDisplayString != nullptr) ? S_OK : E_OUTOFMEMORY;
	}

	return hr;
}

// HelloProperty::RuntimeClassInitialize():
//
// Initializes the 'Hello' property that we are adding to process.
//
HRESULT HelloProperty::RuntimeClassInitialize()
{
	HRESULT hr = S_OK;

	//
	// Create the model for the "type" we are returning from this property.
	//
	ComPtr<HelloModel> spHelloModel;
	IfFailedReturn(MakeAndInitialize<HelloModel>(&spHelloModel));

	IfFailedReturn(GetManager()->CreateDataModelObject(spHelloModel.Get(), &m_spHelloType));

	//
	// Create a new property 'World' and place it on the model
	//
	ComPtr<WorldProperty> spWorldProperty;
	IfFailedReturn(MakeAndInitialize<WorldProperty>(&spWorldProperty, m_spHelloType.Get()));

	ComPtr<IModelObject> spWorldPropertyObject;
	IfFailedReturn(CreateProperty(spWorldProperty.Get(), &spWorldPropertyObject));
	IfFailedReturn(m_spHelloType->SetKey(L"World", spWorldPropertyObject.Get(), nullptr));

	//
	// Create a new property 'Test' and place it on the model.
	//
	ComPtr<TestProperty> spTestProperty;
	IfFailedReturn(MakeAndInitialize<TestProperty>(&spTestProperty));

	ComPtr<IModelObject> spTestPropertyObject;
	IfFailedReturn(CreateProperty(spTestProperty.Get(), &spTestPropertyObject));
	IfFailedReturn(m_spHelloType->SetKey(L"Test", spTestPropertyObject.Get(), nullptr));

	//
	// Create a string conversion and place it on the model.
	//
	ComPtr<HelloStringConversion> spHelloStringConversion;
	IfFailedReturn(MakeAndInitialize<HelloStringConversion>(&spHelloStringConversion, m_spHelloType.Get()));
	IfFailedReturn(m_spHelloType->SetConcept(__uuidof(IStringDisplayableConcept),
		static_cast<IStringDisplayableConcept *>(spHelloStringConversion.Get()),
		nullptr));

	return hr;
}

HRESULT HelloProperty::GetValue(_In_ PCWSTR /*pwszKey*/,                        // Always "Hello" for this
	_In_opt_ IModelObject * /*pProcessInstance*/,   // The process instance 
	_COM_Outptr_ IModelObject **ppValue)
{
	HRESULT hr = S_OK;
	*ppValue = nullptr;

	//
	// Create an instance of the "World" type with some data backing it.
	// 
	ComPtr<IModelObject> spNewInstance;
	IfFailedReturn(GetManager()->CreateSyntheticObject(nullptr, &spNewInstance));

	ComPtr<HelloData> spInstanceData;
	IfFailedReturn(MakeAndInitialize<HelloData>(&spInstanceData, L"Hello World"));
	IfFailedReturn(spNewInstance->AddParentModel(m_spHelloType.Get(), nullptr, false));
	IfFailedReturn(spNewInstance->SetContextForDataModel(m_spHelloType.Get(), spInstanceData.Get()));

	*ppValue = spNewInstance.Detach();
	return hr;
}

HRESULT HelloExtension::Initialize()
{
	HRESULT hr = S_OK;

	//
	// Get access to what is registered under "Debugger.Models.Process" and extend it.
	//
	IfFailedReturn(GetManager()->AcquireNamedModel(L"Debugger.Models.Process", &m_spProcessModelObject));

	//
	// Create a new object which will be added as a parent model to "Debugger.Models.Process".  This new object
	// will have all our extensibility points placed on it.  The singular link between the process model and this
	// object makes it very easy to both add our extension and remove our extension.
	//
	// Any object which is added in the parent model hierarchy must have a data model concept added. 
	//
	ComPtr<HelloExtensionModel> spHelloExtensionModel;
	IfFailedReturn(MakeAndInitialize<HelloExtensionModel>(&spHelloExtensionModel));

	ComPtr<IModelObject> spHelloExtensionModelObject;
	IfFailedReturn(GetManager()->CreateDataModelObject(spHelloExtensionModel.Get(), &spHelloExtensionModelObject));

	//
	// Create a new property 'Hello' and place it on the extension model.
	//
	ComPtr<HelloProperty> spHelloProperty;
	IfFailedReturn(MakeAndInitialize<HelloProperty>(&spHelloProperty));

	ComPtr<IModelObject> spHelloPropertyObject;
	IfFailedReturn(CreateProperty(spHelloProperty.Get(), &spHelloPropertyObject));
	IfFailedReturn(spHelloExtensionModelObject->SetKey(L"Hello", spHelloPropertyObject.Get(), nullptr));

	IfFailedReturn(m_spProcessModelObject->AddParentModel(spHelloExtensionModelObject.Get(), nullptr, false));
	m_spHelloExtensionModelObject = std::move(spHelloExtensionModelObject);

	return hr;
}

void HelloExtension::Uninitialize()
{
	if (m_spProcessModelObject != nullptr && m_spHelloExtensionModelObject != nullptr)
	{
		(void)m_spProcessModelObject->RemoveParentModel(m_spHelloExtensionModelObject.Get());
		m_spProcessModelObject = nullptr;
		m_spHelloExtensionModelObject = nullptr;
	}
}

